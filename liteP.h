//
// Created by Qpromax on 2025/11/18.
//

#pragma once

#include <cstdint>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include "SDL3/SDL.h"
#include "OpenGL/gl3.h"

#include <iostream>
#include <deque>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <expected>

// TODO:    consider c port
// TODO:    consider concept
// TODO:    consider module
namespace liteP {
    //==================================================================================================================
    template<typename T>
    struct TSDeque {
    private:
        std::deque<T> data_;
        mutable std::mutex mtx_;
        std::condition_variable cond_;
        const size_t max_size_;
        bool on_off_ = true;

    public:
        explicit TSDeque(size_t maxSize = 60) : max_size_(maxSize) {}

        // TODO:    consider forward
        bool push(T item)
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cond_.wait(lock, [&]{ return data_.size() < max_size_ || !on_off_; });
            if (!on_off_)
            {
                return false;
            }
            // data.push_back(item);
            data_.emplace_back(std::move(item));
            cond_.notify_one();
            return true;
        }

        // TODO:    consider non-optional/expected
        // TODO:    consider move
        auto front_pop() -> std::optional<T>
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cond_.wait(lock, [&]{ return !data_.empty() || !on_off_; });
            if (data_.empty())
            {
                return std::nullopt;
            }
            std::optional<T> item = data_.front();
            data_.pop_front();
            cond_.notify_one();
            return item;
        }

        // TODO:    consider optional/expected
        auto front_view() const -> T
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (data_.empty())
            {
                return T();
            }
            return data_.front();
        }

        size_t size() const
        {
            std::lock_guard<std::mutex> lock(mtx_);
            return data_.size();
        }

        // NOTE:    only clear the queue, not free the ptrs
        void clear()
        {
            std::lock_guard<std::mutex> lock(mtx_);
            while (!data_.empty())
            {
                // TODO: consider use custom pop
                data_.pop_front();
            }
            cond_.notify_all();
        }

        void close()
        {
            {
                std::lock_guard<std::mutex> lock(mtx_);
                if (!on_off_)
                {
                    return;
                }
                on_off_ = false;
            }
            cond_.notify_all();
        }

        bool on_off() const
        {
            std::lock_guard<std::mutex> lock(mtx_);
            return on_off_;
        }
    };




    //==================================================================================================================
    // TODO:   consider vulkan
    class Renderer {
    private:
        int width = 0;
        int height = 0;
        GLuint textures[3] = {0,0,0};
        GLuint shaderProgram = 0;
        GLuint VAO = 0, VBO = 0;

    public:
        ~Renderer()
        {
            glDeleteTextures(3, textures);
            glDeleteProgram(shaderProgram);
            glDeleteBuffers(1, &VBO);
            glDeleteVertexArrays(1, &VAO);
        }

        // 初始化：纹理 + shader + 顶点数据
        bool init(int w, int h, const char* vertSrc, const char* fragSrc)
        {
            width = w;
            height = h;

            // 创建 YUV 纹理
            glGenTextures(3, textures);
            for (int i = 0; i < 3; ++i)
            {
                glBindTexture(GL_TEXTURE_2D, textures[i]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                if (i == 0) // Y plane
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
                else // U / V plane
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width/2, height/2, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
            }

            // 编译 shader
            shaderProgram = compileShader(vertSrc, fragSrc);
            if (shaderProgram == 0) return false;

            // 顶点数据（屏幕四边形）
            float vertices[] = {
                // pos       // tex
                -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
                 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
                 1.0f, -1.0f, 0.0f,  1.0f, 0.0f
            };

            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
            glBindVertexArray(0);

            return true;
        }

        // 渲染一帧
        void renderFrame(AVFrame* frame)
        {
            if (!frame) return;

            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

            // 更新纹理数据
            glBindTexture(GL_TEXTURE_2D, textures[0]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                            GL_RED, GL_UNSIGNED_BYTE, frame->data[0]);

            glBindTexture(GL_TEXTURE_2D, textures[1]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width/2, height/2,
                            GL_RED, GL_UNSIGNED_BYTE, frame->data[1]);

            glBindTexture(GL_TEXTURE_2D, textures[2]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width/2, height/2,
                            GL_RED, GL_UNSIGNED_BYTE, frame->data[2]);

            // 使用 shader
            glUseProgram(shaderProgram);
            // 绑定纹理单元
            glUniform1i(glGetUniformLocation(shaderProgram, "texY"), 0);
            glUniform1i(glGetUniformLocation(shaderProgram, "texU"), 1);
            glUniform1i(glGetUniformLocation(shaderProgram, "texV"), 2);

            // 绘制四边形
            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);
        }

    private:
        GLuint compileShader(const char* vertSrc, const char* fragSrc)
        {
            auto compile = [](GLenum type, const char* src) -> GLuint {
                GLuint shader = glCreateShader(type);
                glShaderSource(shader, 1, &src, nullptr);
                glCompileShader(shader);
                GLint success;
                glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
                if (!success) {
                    char info[512];
                    glGetShaderInfoLog(shader, 512, nullptr, info);
                    std::cerr << "Shader compile error: " << info << std::endl;
                    return 0;
                }
                return shader;
            };

            GLuint vert = compile(GL_VERTEX_SHADER, vertSrc);
            GLuint frag = compile(GL_FRAGMENT_SHADER, fragSrc);
            if (!vert || !frag) return 0;

            GLuint program = glCreateProgram();
            glAttachShader(program, vert);
            glAttachShader(program, frag);
            glLinkProgram(program);

            GLint success;
            glGetProgramiv(program, GL_LINK_STATUS, &success);
            if (!success)
            {
                char info[512];
                glGetProgramInfoLog(program, 512, nullptr, info);
                std::cerr << "Shader link error: " << info << std::endl;
                return 0;
            }

            glDeleteShader(vert);
            glDeleteShader(frag);
            return program;
        }
    };




    //==================================================================================================================
    class Demux {
    private:
        // AVFormatContext* fmtCtx = nullptr;
        // TODO:    simplify deleter
        std::unique_ptr<AVFormatContext, void(*)(AVFormatContext*)> fmt_ctx_ptr_{
            nullptr,
            [](AVFormatContext* p){;
                if (p != nullptr)
                {
                    avformat_close_input(&p);
                }
            }};
        TSDeque<std::unique_ptr<AVPacket, void(*)(AVPacket*)>>& video_queue_;
        TSDeque<std::unique_ptr<AVPacket, void(*)(AVPacket*)>>& audio_queue_;
        std::jthread thread_;
        const char* path_ = nullptr;
        int video_stream_index_ = -1;
        int audio_stream_index_ = -1;

    public:
        explicit Demux(TSDeque<std::unique_ptr<AVPacket, void(*)(AVPacket*)>>& vq,
                        TSDeque<std::unique_ptr<AVPacket, void(*)(AVPacket*)>>& aq,
                        const char* path)
            :video_queue_(vq), audio_queue_(aq), path_(path)
        {}
        ~Demux()
        {
            stop();
            // avformat_close_input(&fmtCtx);
        }

        void init()
        {
            fmt_ctx_ptr_ = open_input(path_);

            if (avformat_find_stream_info(fmt_ctx_ptr_.get(), nullptr) < 0)
            {
                std::cerr << "Demux could not find stream info";
            }

            for (int i = 0; i < fmt_ctx_ptr_->nb_streams; ++i)
            {
                if (fmt_ctx_ptr_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
                    video_stream_index_ = i;
                if (fmt_ctx_ptr_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
                    audio_stream_index_ = i;
            }
        }

        void run()
        {
            thread_ = std::jthread([this](std::stop_token st){
                task(std::move(st));
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
        auto open_input(const char* pt) -> std::unique_ptr<AVFormatContext, void(*)(AVFormatContext*)>
        {
            AVFormatContext* raw = avformat_alloc_context();
            if (const int ret = avformat_open_input(&raw, pt, nullptr, nullptr);ret < 0)
            {
                return {nullptr,nullptr};
            }
            return {
                raw,
                [](AVFormatContext* p){;
                    if (p != nullptr)
                    {
                        avformat_close_input(&p);
                    }
                }};
        }

        void task(std::stop_token st)
        {
            while (!st.stop_requested())
            {
                std::unique_ptr<AVPacket, void(*)(AVPacket*)> pktPtr(
                    av_packet_alloc(),
                    [](AVPacket* p) {
                            if (p != nullptr)
                            {
                                av_packet_free(&p);
                            }
                        });

                if (pktPtr == nullptr)
                {
                    break;
                }

                if (const int ret = av_read_frame(fmt_ctx_ptr_.get(), pktPtr.get());ret < 0)
                {
                    break;
                }

                // TODO:    consider switch
                if (pktPtr->stream_index == video_stream_index_)
                {
                    video_queue_.push(std::move(pktPtr));
                }
                else if (pktPtr->stream_index == audio_stream_index_)
                {
                    audio_queue_.push(std::move(pktPtr));
                }
                // ignore non-video/audio packets
            }

            video_queue_.close();
            audio_queue_.close();
        }
    };




    //==================================================================================================================
    class Decode {
    private:
        // TODO:    simplify deleter
        std::unique_ptr<AVFormatContext, void(*)(AVFormatContext*)> fmt_ctx_ptr_{
            nullptr,
            [](AVFormatContext* p){;
                if (p != nullptr)
                {
                    avformat_close_input(&p);
                }
            }};
        TSDeque<std::unique_ptr<AVPacket, void(*)(AVPacket*)>>& video_queue_;
        std::jthread thread_;

    public:
        explicit Decode(TSDeque<std::unique_ptr<AVPacket, void(*)(AVPacket*)>>& vq)
            : video_queue_(vq)
        {}

        // TODO
        void init(){}

        // TODO
        void run(){}

        // TODO
        void stop(){}

    private:
        // TODO
        void task(std::stop_token st){}

    };




    //==================================================================================================================
    class MP4 {
    private:
        AVFormatContext* fmt_ctx_ = nullptr;
        // std::unique_ptr<AVFormatContext, decltype(&avformat_close_input)> fmt_ctx;
        AVCodecContext* video_codec_ctx_ = nullptr;
        AVCodecContext* audio_codec_ctx_ = nullptr;
        AVPixelFormat pixel_format_ = AV_PIX_FMT_NONE;
        const char* path_ = nullptr;
        int video_stream_index_ = -1;
        int audio_stream_index_ = -1;

    public:
        int height = 0;
        int width = 0;

        ~MP4()
        {
            // close input file/stream
            if (fmt_ctx_) {
                avformat_close_input(&fmt_ctx_);
            }

            // free video codec context
            if (video_codec_ctx_) {
                avcodec_free_context(&video_codec_ctx_);
            }

            // free audio codec context
            if (audio_codec_ctx_) {
                avcodec_free_context(&audio_codec_ctx_);
            }
        }

        int init(const char* new_path)
        {
            if (avformat_open_input(&fmt_ctx_, new_path, nullptr, nullptr) < 0)
            {
                std::cerr << "MP4 could not open file\n" << std::endl;
                return -1;
            }
            if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0)
            {
                std::cerr << "MP4 could not find stream info\n" << std::endl;
                return -2;
            }

            for (int i = 0; i < fmt_ctx_->nb_streams; ++i)
            {
                if (fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
                    video_stream_index_ = fmt_ctx_->streams[i]->index;
                if (fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
                    audio_stream_index_ = fmt_ctx_->streams[i]->index;
            }
            if (video_stream_index_ == -1)
            {
                std::cerr << "Could not find video stream\n" << std::endl;
                return -3;
            }
            // if (audio_stream_index == -1)
            // {
            // std::cerr << "Could not find audio stream\n" << std::endl;
            // return -3;
            // }
            height = fmt_ctx_->streams[video_stream_index_]->codecpar->height;
            width = fmt_ctx_->streams[video_stream_index_]->codecpar->width;
            return 0;
        }

        // int decode(FrameQueue& frame_queue)
        // {
        //     AVCodecParameters* codecpar = this->fmt_ctx->streams[video_stream_index]->codecpar;
        //
        //     // 查找解码器
        //     const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
        //     if (!codec)
        //     {
        //         std::cerr << "Could not find codec\n";
        //         return -1;
        //     }
        //
        //     // 创建解码器上下文
        //     this->video_codec_ctx = avcodec_alloc_context3(codec);
        //     avcodec_parameters_to_context(this->video_codec_ctx, codecpar);
        //
        //     // 打开解码器
        //     if (avcodec_open2(this->video_codec_ctx, codec, nullptr) < 0)
        //     {
        //         std::cerr << "Could not open codec\n";
        //         return -2;
        //     }
        //
        //     AVPacket* pkt = av_packet_alloc();
        //     AVFrame* frame = av_frame_alloc();
        //
        //     while (av_read_frame(this->fmt_ctx, pkt) >= 0)
        //     {
        //         if (pkt->stream_index == this->video_stream_index)
        //         {
        //             // 视频解码
        //             avcodec_send_packet(this->video_codec_ctx, pkt);
        //             while (avcodec_receive_frame(this->video_codec_ctx, frame) == 0)
        //             {
        //                 frame_queue.push(frame);
        //             }
        //         }
        //         av_packet_unref(pkt);
        //     }
        //     return 0;
        // }
        int decode(liteP::TSDeque<AVFrame*>& frame_queue)
        {
            AVCodecParameters* codecpar = this->fmt_ctx_->streams[video_stream_index_]->codecpar;

            // 查找解码器
            const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
            if (!codec)
            {
                std::cerr << "Could not find codec\n";
                return -1;
            }

            // 创建解码器上下文
            this->video_codec_ctx_ = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(this->video_codec_ctx_, codecpar);

            // 打开解码器
            if (avcodec_open2(this->video_codec_ctx_, codec, nullptr) < 0)
            {
                std::cerr << "Could not open codec\n";
                return -2;
            }

            AVPacket* pkt = av_packet_alloc();
            AVFrame* frame = av_frame_alloc();

            while (av_read_frame(this->fmt_ctx_, pkt) >= 0)
            {
                if (pkt->stream_index == this->video_stream_index_)
                {
                    avcodec_send_packet(this->video_codec_ctx_, pkt);
                    while (avcodec_receive_frame(this->video_codec_ctx_, frame) == 0)
                    {
                        // NOTE:   here is copy
                        AVFrame* f = av_frame_clone(frame);
                        if (!f) {
                            std::cerr << "Failed to clone frame\n";
                            continue;
                        }

                        // TODO:    consider move
                        frame_queue.push(f);

                        //
                        std::cout << "Decoded frame pts: " << f->pts << std::endl;
                    }
                }
                av_packet_unref(pkt);
            }

            av_frame_free(&frame);
            av_packet_free(&pkt);

            return 0;
        }
    };

} // TSDeque



// struct FrameQueue {
// private:
//     std::deque<AVFrame*> queue;
//     mutable std::mutex mtx;
//     std::condition_variable cond;
//     const size_t max_size = 30; // 可调
//
// public:
//     // 阻塞推入帧（生产者使用）
//     void push(AVFrame* frame)
//     {
//         std::unique_lock<std::mutex> lock(mtx);
//         cond.wait(lock, [&]{ return queue.size() < max_size; });
//         queue.push_back(frame);
//         cond.notify_one();
//     }
//
//     // 阻塞弹出帧（消费者使用）
//     AVFrame* pop()
//     {
//         std::unique_lock<std::mutex> lock(mtx);
//         cond.wait(lock, [&]{ return !queue.empty(); });
//         AVFrame* frame = queue.front();
//         queue.pop_front();
//         cond.notify_one();
//         return frame;
//     }
//
//     // 非阻塞获取顶部帧（渲染线程可用）
//     AVFrame* peek_nowait() const
//     {
//         std::lock_guard<std::mutex> lock(mtx);
//         if (queue.empty()) return nullptr;
//         return queue.front();
//     }
//
//     // 获取队列大小
//     size_t size() const
//     {
//         std::lock_guard<std::mutex> lock(mtx);
//         return queue.size();
//     }
//
//     // 清空队列并释放帧
//     void clear()
//     {
//         std::lock_guard<std::mutex> lock(mtx);
//         while (!queue.empty())
//         {
//             AVFrame* f = queue.front();
//             queue.pop_front();
//             av_frame_free(&f);
//         }
//         cond.notify_all();
//     }
// };