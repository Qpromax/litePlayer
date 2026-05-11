#pragma once

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
};

struct av_packet_deleter
{
    void operator()(AVPacket* p) const noexcept
    {
        if (p)
        {
            av_packet_free(&p);
        }
    }
};

struct av_frame_deleter
{
    void operator()(AVFrame* p) const noexcept
    {
        if (p)
        {
            av_frame_free(&p);
        }
    }
};

struct av_codec_context_deleter
{
    void operator()(AVCodecContext* p) const noexcept
    {
        if (p)
        {
            avcodec_free_context(&p);
        }
    }
};

struct av_codec_format_ctx_deleter
{
    void operator()(AVFormatContext* p) const noexcept
    {
        if (p != nullptr)
        {
            avformat_close_input(&p);
        }
    }
};