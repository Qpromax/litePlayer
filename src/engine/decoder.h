#pragma once

extern "C"
{
#include "libavcodec/avcodec.h"
}

#include <memory>
#include <print>
#include <thread>

#include "../utils/ffmpeg_deleter.h"
#include "./queue.h"

class Decoder
{
private:
    // TODO:    simplify deleter
    using ptr_packet_t    = std::unique_ptr<AVPacket, av_packet_deleter>;
    using ptr_frame_t     = std::unique_ptr<AVFrame, av_frame_deleter>;
    using ptr_codec_ctx_t = std::unique_ptr<AVCodecContext, av_codec_context_deleter>;

    ptr_codec_ctx_t            m_ptr_codec_ctx {nullptr};
    QueueAtomic<ptr_packet_t>& m_packet_queue;
    QueueAtomic<ptr_frame_t>&  m_frame_queue;
    std::jthread               m_thread;
    bool                       m_ready = false;

public:
    explicit Decoder(QueueAtomic<ptr_packet_t>& pq, QueueAtomic<ptr_frame_t>& fq, const AVCodecParameters* codecpar)
        : m_packet_queue(pq), m_frame_queue(fq)
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

        m_ptr_codec_ctx.reset(avcodec_alloc_context3(codec));
        if (m_ptr_codec_ctx == nullptr)
        {
            std::print(stderr, "Decode could not allocate codec context\n");
            return;
        }

        int ret = avcodec_parameters_to_context(m_ptr_codec_ctx.get(), codecpar);
        if (ret < 0)
        {
            std::print(stderr, "Decode failed to copy codec parameters to context\n");
            return;
        }

        m_ptr_codec_ctx->thread_count = 0;               // auto threads
        m_ptr_codec_ctx->thread_type  = FF_THREAD_FRAME; // frame parallel

        ret = avcodec_open2(m_ptr_codec_ctx.get(), codec, nullptr);
        if (ret < 0)
        {
            std::print(stderr, "Decode could not open codec\n");
            return;
        }

        m_ready = true;
    }

    Decoder(const Decoder&)              = delete;
    Decoder& operator=(const Decoder&)   = delete;
    Decoder(Decoder&&)                   = delete;
    Decoder& operator=(Decoder&&)        = delete;
    auto     operator<=>(const Decoder&) = delete;

    ~Decoder()
    {
        stop();
    }

    void run()
    {
        if (!m_ready || m_ptr_codec_ctx == nullptr)
        {
            std::print(stderr, "Decode not ready, run() skipped\n");
            return;
        }
        if (m_thread.joinable())
        {
            std::print(stderr, "Demux already running, run() skipped\n");
            return;
        }
        m_thread = std::jthread(
            [this](const std::stop_token& st)
            {
                task(st);
            });
    }

    void stop()
    {
        if (m_thread.joinable())
        {
            m_thread.request_stop();
            m_packet_queue.close();
            m_frame_queue.close();
            m_thread.join();
        }
    }

private:
    void task(const std::stop_token& st)
    {
        while (st.stop_requested() == false)
        {
            auto pkt_opt = m_packet_queue.pop();
            if (pkt_opt == std::nullopt)
            {
                break; // upstream closed
            }

            auto& pkt = *pkt_opt;

            const int ret_send = avcodec_send_packet(m_ptr_codec_ctx.get(), pkt.get());
            if (ret_send < 0)
            {
                // bad packet or decoder state; skip this packet
                continue;
            }

            while (st.stop_requested() == false)
            {
                ptr_frame_t frame(av_frame_alloc());
                if (frame == nullptr)
                {
                    m_frame_queue.close();
                    return;
                }

                const int ret_recv = avcodec_receive_frame(m_ptr_codec_ctx.get(), frame.get());

                if (ret_recv == AVERROR(EAGAIN) || ret_recv == AVERROR_EOF)
                {
                    break; // decoder drained
                }
                if (ret_recv < 0)
                {
                    std::print(stderr, "Decode error for current packet\n");
                    break;
                }

                if (m_frame_queue.push(std::move(frame)) == false)
                {
                    m_frame_queue.close(); // downstream closed
                    return;
                }
            }
        }

        // flush delayed frames
        avcodec_send_packet(m_ptr_codec_ctx.get(), nullptr);
        while (st.stop_requested() == false)
        {
            ptr_frame_t frame(av_frame_alloc());
            if (frame == nullptr)
            {
                break;
            }

            const int ret_recv = avcodec_receive_frame(m_ptr_codec_ctx.get(), frame.get());
            if (ret_recv == AVERROR(EAGAIN) || ret_recv == AVERROR_EOF)
            {
                break;
            }
            if (ret_recv < 0)
            {
                std::print(stderr, "Decode error for current packet\n");
                break;
            }

            if (m_frame_queue.push(std::move(frame)) == false)
            {
                break;
            }
        }

        m_frame_queue.close();
    }
};