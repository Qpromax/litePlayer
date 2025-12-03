//
// Created by Qpromax on 2025/11/18.
//

#pragma once

#include <deque>
#include <condition_variable>
#include <mutex>

namespace liteP {
    //==================================================================================================================
    template<typename T>
    struct TSDeque {
    private:
        std::deque<T> data;
        mutable std::mutex mtx;
        std::condition_variable cond;
        const size_t maxSize;
        bool onOff = true;
    public:
        explicit TSDeque(size_t maxSize = 60) : maxSize(maxSize) {}
        ~TSDeque() = default;

        TSDeque(const TSDeque&) = delete;
        TSDeque& operator=(const TSDeque&) = delete;
        TSDeque(TSDeque&&) = delete;
        TSDeque& operator=(TSDeque&&) = delete;

        void push(T item)
        {
            std::unique_lock<std::mutex> lock(mtx);
            cond.wait(lock, [&]{ return data.size() < maxSize; });
            data.push_back(item);
            cond.notify_one();
        }

        std::optional<T> front_pop()
        {
            std::unique_lock<std::mutex> lock(mtx);
            cond.wait(lock, [&]{ return !data.empty() || !onOff; });
            if (data.empty()) return std::nullopt;
            std::optional<T> item = data.front();
            data.pop_front();
            cond.notify_one();
            return item;
        }

        T front_view()
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (data.empty()) return T();
            return data.front();
        }

        size_t size() const
        {
            std::lock_guard<std::mutex> lock(mtx);
            return data.size();
        }

        // 仅弹出，自行处理释放
        void clear()
        {
            std::lock_guard<std::mutex> lock(mtx);
            while (!data.empty()) data.pop();
            cond.notify_all();
        }

        void close()
        {
            std::lock_guard<std::mutex> lock(mtx);
            onOff = false;
        }

        bool on_off() const
        {
            std::lock_guard<std::mutex> lock(mtx);
            return onOff;
        }
    };




    //==================================================================================================================
    class Renderer {
    private:
        int width = 0;
        int height = 0;
        GLuint textures[3] = {0,0,0};
        GLuint shaderProgram = 0;
        GLuint VAO = 0, VBO = 0;

    public:
        Renderer() = default;
        ~Renderer()
        {
            glDeleteTextures(3, textures);
            glDeleteProgram(shaderProgram);
            glDeleteBuffers(1, &VBO);
            glDeleteVertexArrays(1, &VAO);
        }

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

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
        const char* path = nullptr;
        AVFormatContext* fmtCtx = nullptr;
        std::jthread thread;
        TSDeque<AVPacket>& videoQueue;
        TSDeque<AVPacket>& audioQueue;
        int videoStreamIndex = -1;
        int audioStreamIndex = -1;

    public:
        Demux(TSDeque<AVPacket>& vq, TSDeque<AVPacket>& aq, const char* path)
            :videoQueue(vq), audioQueue(aq), path(path)
        {

        }
        ~Demux()
        {
            stop();
            avformat_close_input(&fmtCtx);
        }

        Demux(const Demux&) = delete;
        Demux& operator=(const Demux&) = delete;
        Demux(Demux&&) = delete;
        Demux& operator=(Demux&&) = delete;

        void init()
        {
            if (avformat_open_input(&fmtCtx, path, nullptr, nullptr) < 0)
                throw std::runtime_error("Could not open file");

            if (avformat_find_stream_info(fmtCtx, nullptr) < 0)
                throw std::runtime_error("Could not find stream info");

            for (int i = 0; i < fmtCtx->nb_streams; ++i)
            {
                if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
                    videoStreamIndex = i;
                if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
                    audioStreamIndex = i;
            }
        }

        void run()
        {
            thread = std::jthread([this](std::stop_token st){
                task(st);
            });
        }

        void stop()
        {
            if (thread.joinable())
            {
                videoQueue.close();
                audioQueue.close();
                thread.request_stop();
                thread.join();
            }
        }

    private:
        void task(std::stop_token st)
        {
            AVPacket pkt;
            av_init_packet(&pkt);

            while (!st.stop_requested())
            {
                int ret = av_read_frame(fmtCtx, &pkt);
                if (ret < 0) break;

                if (pkt.stream_index == videoStreamIndex) videoQueue.push(pkt);
                else if (pkt.stream_index == audioStreamIndex) audioQueue.push(pkt);
                else av_packet_unref(&pkt);
            }
            videoQueue.close();
            audioQueue.close();
        }
    };




    //==================================================================================================================
    class Decode {
    private:
    public:
        Decode() = default;
        ~Decode() = default;

        Decode(const Decode&) = delete;
        Decode& operator=(const Decode&) = delete;
        Decode(Decode&&) = delete;
        Decode& operator=(Decode&&) = delete;


    };




    //==================================================================================================================
    class MP4 {
    private:
        const char* path = nullptr;
        AVFormatContext* fmt_ctx = nullptr;
        // std::unique_ptr<AVFormatContext, decltype(&avformat_close_input)> fmt_ctx;

        int video_stream_index = -1;
        AVCodecContext* video_codec_ctx = nullptr;
        AVPixelFormat pixel_format = AV_PIX_FMT_NONE;

        int audio_stream_index = -1;
        AVCodecContext* audio_codec_ctx = nullptr;

    public:
        int height = 0;
        int width = 0;

        MP4() = default;
        ~MP4()
        {
            // 关闭输入文件/流
            if (fmt_ctx) {
                avformat_close_input(&fmt_ctx);
            }

            // 释放视频解码上下文
            if (video_codec_ctx) {
                avcodec_free_context(&video_codec_ctx);
            }

            // 释放音频解码上下文
            if (audio_codec_ctx) {
                avcodec_free_context(&audio_codec_ctx);
            }
        }

        MP4(const MP4&) = delete;
        MP4& operator=(const MP4&) = delete;
        MP4(MP4&&) = delete;
        MP4& operator=(MP4&&) = delete;

        int init(const char* new_path)
        {
            // open file
            if (avformat_open_input(&fmt_ctx, new_path, nullptr, nullptr) < 0)
            {
                std::cerr << "Could not open file\n" << std::endl;
                return -1;
            }
            if (avformat_find_stream_info(fmt_ctx, nullptr) < 0)
            {
                std::cerr << "Could not find stream info\n" << std::endl;
                return -2;
            }

            // find video and audio stream
            for (int i = 0; i < fmt_ctx->nb_streams; ++i)
            {
                if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
                    video_stream_index = fmt_ctx->streams[i]->index;
                if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
                    audio_stream_index = fmt_ctx->streams[i]->index;
            }
            if (video_stream_index == -1)
            {
                std::cerr << "Could not find video stream\n" << std::endl;
                return -3;
            }
            // if (audio_stream_index == -1)
            // {
            // std::cerr << "Could not find audio stream\n" << std::endl;
            // return -3;
            // }
            height = fmt_ctx->streams[video_stream_index]->codecpar->height;
            width = fmt_ctx->streams[video_stream_index]->codecpar->width;
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
            AVCodecParameters* codecpar = this->fmt_ctx->streams[video_stream_index]->codecpar;

            // 查找解码器
            const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
            if (!codec)
            {
                std::cerr << "Could not find codec\n";
                return -1;
            }

            // 创建解码器上下文
            this->video_codec_ctx = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(this->video_codec_ctx, codecpar);

            // 打开解码器
            if (avcodec_open2(this->video_codec_ctx, codec, nullptr) < 0)
            {
                std::cerr << "Could not open codec\n";
                return -2;
            }

            AVPacket* pkt = av_packet_alloc();
            AVFrame* frame = av_frame_alloc();

            while (av_read_frame(this->fmt_ctx, pkt) >= 0)
            {
                if (pkt->stream_index == this->video_stream_index)
                {
                    avcodec_send_packet(this->video_codec_ctx, pkt);
                    while (avcodec_receive_frame(this->video_codec_ctx, frame) == 0)
                    {
                        // 克隆一份帧数据，避免重复使用同一个 frame
                        AVFrame* f = av_frame_clone(frame);
                        if (!f) {
                            std::cerr << "Failed to clone frame\n";
                            continue;
                        }

                        // 推入队列
                        frame_queue.push(f);

                        // 打印 pts 方便调试
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