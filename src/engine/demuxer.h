#pragma once

extern "C"
{
#include "libavcodec/packet.h"
#include "libavformat/avformat.h"
}
#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <memory>
#include <print>

#include "../utils/ffmpeg_deleter.h"
#include "./queue.h"

class Demuxer
{
private:
    using ptr_packet_t     = std::unique_ptr<AVPacket, av_packet_deleter>;
    using ptr_format_ctx_t = std::unique_ptr<AVFormatContext, av_codec_format_ctx_deleter>;

    ptr_format_ctx_t           m_p_format_ctx {nullptr};
    QueueAtomic<ptr_packet_t>& m_video_queue;
    QueueAtomic<ptr_packet_t>& m_audio_queue;
    int                        m_video_stream_index = -1;
    int                        m_audio_stream_index = -1;

public:
    explicit Demuxer(QueueAtomic<ptr_packet_t>& vq, QueueAtomic<ptr_packet_t>& aq, const char* path)
        : m_video_queue(vq), m_audio_queue(aq)
    {
        m_p_format_ctx = open_input(path);
        if (m_p_format_ctx == nullptr)
        {
            std::print(stderr, "[Demux] could not open input\n");
            return;
        }

        if (avformat_find_stream_info(m_p_format_ctx.get(), nullptr) < 0)
        {
            std::print(stderr, "[Demux] could not find stream info\n");
            return;
        }

        m_video_stream_index = av_find_best_stream(m_p_format_ctx.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        m_audio_stream_index = av_find_best_stream(m_p_format_ctx.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

        if (m_video_stream_index < 0)
        {
            std::print(stderr, "[Demux] could not find video stream\n");
            return;
        }
    }

    Demuxer(const Demuxer&)             = delete;
    Demuxer operator=(const Demuxer&)   = delete;
    Demuxer(Demuxer&&)                  = delete;
    Demuxer operator=(Demuxer&&)        = delete;
    auto    operator<=>(const Demuxer&) = delete;

    ~Demuxer() = default;

    auto schedule_run(stdexec::scheduler auto sched)
    {
        return stdexec::schedule(sched)
             | stdexec::let_value(
                   [this]()
                   {
                       return stdexec::get_stop_token();
                   })
             | stdexec::then(
                   [this](auto stop_token)
                   {
                       this->task(stop_token);
                   });
    }

    // info

    [[nodiscard]] const AVCodecParameters* video_codecpar() const
    {
        return m_p_format_ctx->streams[m_video_stream_index]->codecpar;
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
        if (!m_p_format_ctx || m_video_stream_index < 0)
        {
            return AVRational {0, 1};
        }
        return m_p_format_ctx->streams[m_video_stream_index]->time_base;
    }

private:
    [[nodiscard]] static ptr_format_ctx_t open_input(const char* pt)
    {
        AVFormatContext* raw = avformat_alloc_context();
        const int        ret = avformat_open_input(&raw, pt, nullptr, nullptr);
        if (ret < 0)
        {
            return {nullptr};
        }

        return ptr_format_ctx_t {raw};
    }

    void task(auto stop_token)
    {
        while (!stop_token.stop_requested())
        {
            ptr_packet_t ptr_pkt(av_packet_alloc());

            if (ptr_pkt == nullptr)
            {
                break;
            }

            const int ret = av_read_frame(m_p_format_ctx.get(), ptr_pkt.get());
            if (ret < 0)
            {
                break;
            }

            // TODO:    consider switch
            bool pushed = true;
            if (ptr_pkt->stream_index == m_video_stream_index)
            {
                pushed = m_video_queue.push(std::move(ptr_pkt));
            }
            else if (ptr_pkt->stream_index == m_audio_stream_index)
            {
                pushed = m_audio_queue.push(std::move(ptr_pkt));
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

        m_video_queue.close();
        m_audio_queue.close();
    };
};