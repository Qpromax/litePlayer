#pragma once

extern "C"
{
#include "libavcodec/packet.h"
#include "libavformat/avformat.h"
}

#include <memory>
#include <print>
#include <thread>

#include "./queue.h"

class Demux
{
private:
    using packet_ptr_t     = std::unique_ptr<AVPacket, void (*)(AVPacket*)>;
    using format_ctx_ptr_t = std::unique_ptr<AVFormatContext, void (*)(AVFormatContext*)>;

    // AVFormatContext* fmtCtx = nullptr;
    // TODO:    simplify deleter
    format_ctx_ptr_t format_ctx_ptr_ {nullptr,
                                      [](AVFormatContext* p)
                                      {
                                          if (p != nullptr)
                                          {
                                              avformat_close_input(&p);
                                          }
                                      }};

    TSDeque<packet_ptr_t>& video_queue_;
    TSDeque<packet_ptr_t>& audio_queue_;
    std::jthread           thread_;
    int                    video_stream_index_ = -1;
    int                    audio_stream_index_ = -1;
    bool                   ready_              = false;

public:
    explicit Demux(TSDeque<packet_ptr_t>& vq, TSDeque<packet_ptr_t>& aq, const char* path)
        : video_queue_(vq), audio_queue_(aq)
    {
        format_ctx_ptr_ = open_input(path);
        if (format_ctx_ptr_ == nullptr)
        {
            std::print(stderr, "Demux could not open input\n");
            return;
        }

        if (avformat_find_stream_info(format_ctx_ptr_.get(), nullptr) < 0)
        {
            std::print(stderr, "Demux could not find stream info\n");
            return;
        }

        video_stream_index_ = av_find_best_stream(format_ctx_ptr_.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        audio_stream_index_ = av_find_best_stream(format_ctx_ptr_.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

        if (video_stream_index_ < 0)
        {
            std::print(stderr, "Demux could not find video stream\n");
            return;
        }

        ready_ = true;
    }

    Demux(const Demux&)             = delete;
    Demux operator=(const Demux&)   = delete;
    Demux(Demux&&)                  = delete;
    Demux operator=(Demux&&)        = delete;
    auto  operator<=>(const Demux&) = delete;

    ~Demux()
    {
        stop();
        // avformat_close_input(&fmtCtx);
    }

    [[nodiscard]] bool ready() const
    {
        return ready_;
    }

    [[nodiscard]] const AVCodecParameters* video_codecpar() const
    {
        if (!ready_ || !format_ctx_ptr_ || video_stream_index_ < 0)
        {
            return nullptr;
        }
        return format_ctx_ptr_->streams[video_stream_index_]->codecpar;
    }

    [[nodiscard]] std::pair<int, int> video_size() const
    {
        const AVCodecParameters* cp = video_codecpar();
        if (cp == nullptr)
        {
            return {0, 0};
        }
        return {cp->width, cp->height};
    }

    [[nodiscard]] AVRational video_time_base() const
    {
        if (!format_ctx_ptr_ || video_stream_index_ < 0)
        {
            return AVRational {0, 1};
        }
        return format_ctx_ptr_->streams[video_stream_index_]->time_base;
    }

    void run()
    {
        if (ready_ == false || format_ctx_ptr_ == nullptr || video_stream_index_ < 0)
        {
            video_queue_.close();
            audio_queue_.close();
            std::print(stderr, "Decode not ready, run() skipped, queue closed\n");
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
            video_queue_.close();
            audio_queue_.close();
            thread_.request_stop();
            thread_.join();
        }
    }

private:
    [[nodiscard]] static format_ctx_ptr_t open_input(const char* pt)
    {
        AVFormatContext* raw = avformat_alloc_context();
        const int        ret = avformat_open_input(&raw, pt, nullptr, nullptr);
        if (ret < 0)
        {
            return {nullptr,
                    [](AVFormatContext* p)
                    {
                        if (p != nullptr)
                        {
                            avformat_close_input(&p);
                        }
                    }};
        }
        return {raw,
                [](AVFormatContext* p)
                {
                    if (p != nullptr)
                    {
                        avformat_close_input(&p);
                    }
                }};
    }

    void task(const std::stop_token& st)
    {
        while (st.stop_requested() == false)
        {
            packet_ptr_t pkt_ptr(av_packet_alloc(),
                                 [](AVPacket* p)
                                 {
                                     if (p != nullptr)
                                     {
                                         av_packet_free(&p);
                                     }
                                 });

            if (pkt_ptr == nullptr)
            {
                break;
            }

            const int ret = av_read_frame(format_ctx_ptr_.get(), pkt_ptr.get());
            if (ret < 0)
            {
                break;
            }

            // TODO:    consider switch
            bool pushed = true;
            if (pkt_ptr->stream_index == video_stream_index_)
            {
                pushed = video_queue_.push(std::move(pkt_ptr));
            }
            else if (pkt_ptr->stream_index == audio_stream_index_)
            {
                pushed = audio_queue_.push(std::move(pkt_ptr));
            }
            else
            {
                continue;
                // ignore non-video/audio packets
            }

            if (pushed == false)
            {
                break; // downstream closed
            }
        }

        video_queue_.close();
        audio_queue_.close();
    };
};