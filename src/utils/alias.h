#pragma once

extern "C"
{
#include "libavcodec/packet.h"
#include "libavformat/avformat.h"
}
#include <memory>

#include "./ffmpeg_deleter.h"

using ptr_packet_t     = std::unique_ptr<AVPacket, av_packet_deleter>;
using ptr_frame_t      = std::unique_ptr<AVFrame, av_frame_deleter>;
using ptr_format_ctx_t = std::unique_ptr<AVFormatContext, av_codec_format_ctx_deleter>;
using ptr_codec_ctx_t  = std::unique_ptr<AVCodecContext, av_codec_context_deleter>;
