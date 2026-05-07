#pragma once

extern "C"
{
#include "libavcodec/avcodec.h"
}

#include <memory>
#include <print>
#include <thread>

#include "./queue.h"

class Decode
{
private:
    // TODO:    simplify deleter
    using packet_ptr_t    = std::unique_ptr<AVPacket, void (*)(AVPacket*)>;
    using frame_ptr_t     = std::unique_ptr<AVFrame, void (*)(AVFrame*)>;
    using codec_ctx_ptr_t = std::unique_ptr<AVCodecContext, void (*)(AVCodecContext*)>;

    codec_ctx_ptr_t codec_ctx_ptr_ {nullptr,
                                    [](AVCodecContext* p)
                                    {
                                        if (p != nullptr)
                                        {
                                            avcodec_free_context(&p);
                                        }
                                    }};

    TSDeque<packet_ptr_t>& packet_queue_;
    TSDeque<frame_ptr_t>&  frame_queue_;
    std::jthread           thread_;
    bool                   ready_ = false;

public:
    explicit Decode(TSDeque<packet_ptr_t>& pq, TSDeque<frame_ptr_t>& fq, const AVCodecParameters* codecpar)
        : packet_queue_(pq), frame_queue_(fq)
    {
        if (codecpar == nullptr)
        {
            std::print(stderr, "Decode got null codec parameters\n");
            return;
        }

        const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
        if (codec == nullptr)
        {
            std::print(stderr, "Decode could not find decoder\n");
            return;
        }

        codec_ctx_ptr_.reset(avcodec_alloc_context3(codec));
        if (codec_ctx_ptr_ == nullptr)
        {
            std::print(stderr, "Decode could not allocate codec context\n");
            return;
        }

        int ret = avcodec_parameters_to_context(codec_ctx_ptr_.get(), codecpar);
        if (ret < 0)
        {
            std::print(stderr, "Decode failed to copy codec parameters to context\n");
            return;
        }

        codec_ctx_ptr_->thread_count = 0;               // auto threads
        codec_ctx_ptr_->thread_type  = FF_THREAD_FRAME; // frame parallel

        ret = avcodec_open2(codec_ctx_ptr_.get(), codec, nullptr);
        if (ret < 0)
        {
            std::print(stderr, "Decode could not open codec\n");
            return;
        }

        ready_ = true;
    }

    Decode(const Decode&)              = delete;
    Decode& operator=(const Decode&)   = delete;
    Decode(Decode&&)                   = delete;
    Decode& operator=(Decode&&)        = delete;
    auto    operator<=>(const Decode&) = delete;

    ~Decode()
    {
        stop();
    }

    void run()
    {
        if (!ready_ || codec_ctx_ptr_ == nullptr)
        {
            std::print(stderr, "Decode not ready, run() skipped\n");
            return;
        }
        if (thread_.joinable())
        {
            std::print(stderr, "Demux already running, run() skipped\n");
            return;
        }
        thread_ = std::jthread(
            [this](const std::stop_token& st)
            {
                task(st);
            });
    }

    void stop()
    {
        if (thread_.joinable())
        {
            thread_.request_stop();
            packet_queue_.close();
            frame_queue_.close();
            thread_.join();
        }
    }

private:
    void task(const std::stop_token& st)
    {
        while (st.stop_requested() == false)
        {
            auto pkt_opt = packet_queue_.pop_front();
            if (pkt_opt == std::nullopt)
            {
                break; // upstream closed
            }

            auto& pkt = *pkt_opt;

            const int ret_send = avcodec_send_packet(codec_ctx_ptr_.get(), pkt.get());
            if (ret_send < 0)
            {
                // bad packet or decoder state; skip this packet
                continue;
            }

            while (st.stop_requested() == false)
            {
                frame_ptr_t frame(av_frame_alloc(),
                                  [](AVFrame* f)
                                  {
                                      if (f != nullptr)
                                      {
                                          av_frame_free(&f);
                                      }
                                  });
                if (frame == nullptr)
                {
                    frame_queue_.close();
                    return;
                }

                const int ret_recv = avcodec_receive_frame(codec_ctx_ptr_.get(), frame.get());

                if (ret_recv == AVERROR(EAGAIN) || ret_recv == AVERROR_EOF)
                {
                    break; // decoder drained
                }
                if (ret_recv < 0)
                {
                    std::print(stderr, "Decode error for current packet\n");
                    break;
                }

                if (frame_queue_.push(std::move(frame)) == false)
                {
                    frame_queue_.close(); // downstream closed
                    return;
                }
            }
        }

        // flush delayed frames
        avcodec_send_packet(codec_ctx_ptr_.get(), nullptr);
        while (st.stop_requested() == false)
        {
            frame_ptr_t frame(av_frame_alloc(),
                              [](AVFrame* f)
                              {
                                  if (f != nullptr)
                                  {
                                      av_frame_free(&f);
                                  }
                              });
            if (frame == nullptr)
            {
                break;
            }

            const int ret_recv = avcodec_receive_frame(codec_ctx_ptr_.get(), frame.get());
            if (ret_recv == AVERROR(EAGAIN) || ret_recv == AVERROR_EOF)
            {
                break;
            }
            if (ret_recv < 0)
            {
                std::print(stderr, "Decode error for current packet\n");
                break;
            }

            if (frame_queue_.push(std::move(frame)) == false)
            {
                break;
            }
        }

        frame_queue_.close();
    }
};