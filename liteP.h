//
// Created by Qpromax on 2025/11/18.
//

#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
}

#include <SDL3/SDL.h>

#ifdef _WIN32
#include <windows.h>
#include <GL/gl3.h>
#elif defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl3.h>
#endif

#include <print>
#include <deque>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <concepts>
#include <utility>



namespace liteP {



    //==================================================================================================================
    template<typename T>
    struct TSDeque {
    private:
        std::deque<T> data_;
        mutable std::mutex mtx_;
        std::condition_variable cond_;
        const size_t max_size_;
        bool is_running_ = true;

        // todo: use circle queue

    public:
        explicit TSDeque(size_t maxSize = 60) : max_size_(maxSize) {}

        ~TSDeque() { close(); }


        // todo: add push_batch()
        template<typename Y>
            requires std::constructible_from<T, Y&&>
        bool push(Y&& item)
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cond_.wait(lock, [&]{ return data_.size() < max_size_ || !is_running_; });
            if (!is_running_)
            {
                return false;
            }
            data_.emplace_back(std::forward<Y>(item));
            lock.unlock();
            cond_.notify_one();
            return true;
        }



        std::optional<T> pop_front()
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cond_.wait(lock, [&]{ return !data_.empty() || !is_running_; });
            if (data_.empty())
            {
                return std::nullopt;
            }
            std::optional<T> item = (std::move(data_.front()));
            data_.pop_front();
            lock.unlock();
            cond_.notify_one();
            return item;
        }



        // T front_view() const
        // {
        //     std::lock_guard<std::mutex> lock(mtx_);
        //     if (data_.empty()) {
        //         return T();
        //     }
        //     return data_.front();
        // }



        [[nodiscard]] size_t size() const
        {
            std::lock_guard<std::mutex> lock(mtx_);
            return data_.size();
        }

        void clear()
        {
            std::unique_lock<std::mutex> lock(mtx_);
            while (data_.empty() == false) {
                data_.pop_front();
            }
            lock.unlock();
            cond_.notify_all();
        }



        void close()
        {
            // lock space
            {
                std::lock_guard<std::mutex> lock(mtx_);
                if (is_running_ == false){
                    return;
                }
                is_running_ = false;
            }
            cond_.notify_all();
        }



        [[nodiscard]] bool is_running() const
        {
            std::lock_guard<std::mutex> lock(mtx_);
            return is_running_;
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
        GLint texYLoc_ = -1;
        GLint texULoc_ = -1;
        GLint texVLoc_ = -1;
        bool init_ok_ = false;

    public:
        explicit Renderer(int w, int h, const char* vertSrc, const char* fragSrc)
        {
            init_ok_ = init(w, h, vertSrc, fragSrc);
        }

        ~Renderer()
        {
            cleanup();
        }

        [[nodiscard]] bool ok() const
        {
            return init_ok_;
        }

        void shutdown()
        {
            cleanup();
        }

        bool init(int w, int h, const char* vertSrc, const char* fragSrc)
        {
            init_ok_ = false;
            cleanup();

            width = w;
            height = h;

            // YUV texture
            glGenTextures(3, textures);
            for (int i = 0; i < 3; ++i)
            {
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, textures[i]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                if (i == 0) {
                    glTexImage2D(GL_TEXTURE_2D
                                , 0
                                , GL_RED
                                , width, height
                                , 0
                                , GL_RED
                                , GL_UNSIGNED_BYTE
                                , nullptr);
                } else {
                    glTexImage2D(GL_TEXTURE_2D
                                , 0
                                , GL_RED
                                , width / 2, height / 2
                                , 0
                                , GL_RED
                                , GL_UNSIGNED_BYTE
                                , nullptr);
                }
            }

            shaderProgram = compileShader(vertSrc, fragSrc);
            if (shaderProgram == 0) return false;

            float vertices[] = {
                // pos       // tex
                -1.0f,  1.0f, 0.0f,  0.0f, 0.0f,
                -1.0f, -1.0f, 0.0f,  0.0f, 1.0f,
                 1.0f,  1.0f, 0.0f,  1.0f, 0.0f,
                 1.0f, -1.0f, 0.0f,  1.0f, 1.0f
            };

            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3
                                 , GL_FLOAT
                                 , GL_FALSE
                                 , 5 * sizeof(float)
                                 , nullptr);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2
                                 , GL_FLOAT
                                 , GL_FALSE
                                 , 5 * sizeof(float)
                                 , (void*)(3 * sizeof(float)));
            glBindVertexArray(0);

            // fix sampler binding
            glUseProgram(shaderProgram);
            texYLoc_ = glGetUniformLocation(shaderProgram, "texY");
            texULoc_ = glGetUniformLocation(shaderProgram, "texU");
            texVLoc_ = glGetUniformLocation(shaderProgram, "texV");
            if (texYLoc_ >= 0) glUniform1i(texYLoc_, 0);
            if (texULoc_ >= 0) glUniform1i(texULoc_, 1);
            if (texVLoc_ >= 0) glUniform1i(texVLoc_, 2);

            init_ok_ = true;
            return true;
        }

        void renderFrame(AVFrame* frame)
        {
            if (!frame) return;

            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

            // FFmpeg 帧可能带有 stride（linesize），按行长度上传更稳妥
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textures[0]);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[0]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                            GL_RED, GL_UNSIGNED_BYTE, frame->data[0]);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, textures[1]);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[1]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2,
                            GL_RED, GL_UNSIGNED_BYTE, frame->data[1]);

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, textures[2]);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[2]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2,
                            GL_RED, GL_UNSIGNED_BYTE, frame->data[2]);

            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

            glUseProgram(shaderProgram);
            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);
        }

    private:
        void cleanup()
        {
            if (SDL_GL_GetCurrentContext() == nullptr) {
                return;
            }
            if (textures[0] || textures[1] || textures[2]) {
                glDeleteTextures(3, textures);
                textures[0] = textures[1] = textures[2] = 0;
            }
            if (shaderProgram != 0) {
                glDeleteProgram(shaderProgram);
                shaderProgram = 0;
            }
            if (VBO != 0) {
                glDeleteBuffers(1, &VBO);
                VBO = 0;
            }
            if (VAO != 0) {
                glDeleteVertexArrays(1, &VAO);
                VAO = 0;
            }
        }

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
                    std::print(stderr, "Shader compile error: {}", info);
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
                std::print(stderr, "Shader link error: {}", info);
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
        using packet_ptr_t     = std::unique_ptr<AVPacket, void(*)(AVPacket*)>;
        using format_ctx_ptr_t = std::unique_ptr<AVFormatContext, void(*)(AVFormatContext*)>;



        // AVFormatContext* fmtCtx = nullptr;
        // TODO:    simplify deleter
        format_ctx_ptr_t format_ctx_ptr_{
            nullptr
            , [](AVFormatContext* p) {
                if (p != nullptr) {
                    avformat_close_input(&p);
                }
            }};

        TSDeque<packet_ptr_t>& video_queue_;
        TSDeque<packet_ptr_t>& audio_queue_;
        std::jthread thread_;
        int video_stream_index_ = -1;
        int audio_stream_index_ = -1;
        bool ready_ = false;



    public:
        explicit Demux(TSDeque<packet_ptr_t>& vq
                      ,TSDeque<packet_ptr_t>& aq
                      ,const char* path)
            : video_queue_(vq), audio_queue_(aq)
        {
            format_ctx_ptr_ = open_input(path);
            if (format_ctx_ptr_ == nullptr) {
                std::print(stderr, "Demux could not open input\n");
                return;
            }

            if (avformat_find_stream_info(format_ctx_ptr_.get(), nullptr) < 0) {
                std::print(stderr, "Demux could not find stream info\n");
                return;
            }

            video_stream_index_ = av_find_best_stream(
                format_ctx_ptr_.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
            audio_stream_index_ = av_find_best_stream(
                format_ctx_ptr_.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

            if (video_stream_index_ < 0) {
                std::print(stderr, "Demux could not find video stream\n");
                return;
            }

            ready_ = true;
        }

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
            if (!ready_ || !format_ctx_ptr_ || video_stream_index_ < 0) {
                return nullptr;
            }
            return format_ctx_ptr_->streams[video_stream_index_]->codecpar;
        }



        [[nodiscard]] std::pair<int, int> video_size() const
        {
            const AVCodecParameters* cp = video_codecpar();
            if (cp == nullptr) {
                return {0, 0};
            }
            return {cp->width, cp->height};
        }



        [[nodiscard]] AVRational video_time_base() const
        {
            if (!format_ctx_ptr_ || video_stream_index_ < 0) {
                return AVRational{0, 1};
            }
            return format_ctx_ptr_->streams[video_stream_index_]->time_base;
        }



        void run()
        {
            if (ready_ == false || format_ctx_ptr_ == nullptr || video_stream_index_ < 0) {
                video_queue_.close();
                audio_queue_.close();
                std::print(stderr, "Decode not ready, run() skipped, queue closed\n");
                return;
            }
            if (thread_.joinable()) {
                std::print(stderr, "Demux already running, run() skipped\n");
                return;
            }
            thread_ = std::jthread([this](const std::stop_token& st) {
                task(st);
            });
        }




        void stop()
        {
            if (thread_.joinable()) {
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
            const int ret = avformat_open_input(&raw, pt, nullptr, nullptr);
            if (ret < 0) {
                return {nullptr, [](AVFormatContext* p) {
                    if (p != nullptr) {
                        avformat_close_input(&p);
                    }
                }};
            }
            return {
                raw,
                [](AVFormatContext* p) {
                    if (p != nullptr) {
                        avformat_close_input(&p);
                    }
                }};
        }



        void task(const std::stop_token& st)
        {
            while (st.stop_requested() == false)
            {
                packet_ptr_t pkt_ptr(
                    av_packet_alloc()
                    ,[](AVPacket* p) {
                        if (p != nullptr) {
                            av_packet_free(&p);
                        }
                    });

                if (pkt_ptr == nullptr) {
                    break;
                }

                const int ret = av_read_frame(format_ctx_ptr_.get(), pkt_ptr.get());
                if (ret < 0) {
                    break;
                }

                // TODO:    consider switch
                bool pushed = true;
                if (pkt_ptr->stream_index == video_stream_index_) {
                    pushed = video_queue_.push(std::move(pkt_ptr));
                } else if (pkt_ptr->stream_index == audio_stream_index_) {
                    pushed = audio_queue_.push(std::move(pkt_ptr));
                } else {
                    continue;
                    // ignore non-video/audio packets
                }

                if (pushed == false){
                    break; // downstream closed
                }
            }

            video_queue_.close();
            audio_queue_.close();
        }



    };




    //==================================================================================================================
    class Decode {
    private:
        // TODO:    simplify deleter
        using packet_ptr_t    = std::unique_ptr<AVPacket, void(*)(AVPacket*)>;
        using frame_ptr_t     = std::unique_ptr<AVFrame, void(*)(AVFrame*)>;
        using codec_ctx_ptr_t = std::unique_ptr<AVCodecContext, void(*)(AVCodecContext*)>;



        codec_ctx_ptr_t codec_ctx_ptr_{
            nullptr
            , [](AVCodecContext* p){;
                if (p != nullptr)
                {
                    avcodec_free_context(&p);
                }
            }};

        TSDeque<packet_ptr_t>& packet_queue_;
        TSDeque<frame_ptr_t>& frame_queue_;
        std::jthread thread_;
        bool ready_ = false;



    public:
        explicit Decode(TSDeque<packet_ptr_t>& pq
                        , TSDeque<frame_ptr_t>& fq
                        , const AVCodecParameters* codecpar)
            : packet_queue_(pq), frame_queue_(fq)
        {
            if (codecpar == nullptr) {
                std::print(stderr, "Decode got null codec parameters\n");
                return;
            }

            const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
            if (codec == nullptr){
                std::print(stderr, "Decode could not find decoder\n");
                return;
            }

            codec_ctx_ptr_.reset(avcodec_alloc_context3(codec));
            if (codec_ctx_ptr_ == nullptr){
                std::print(stderr, "Decode could not allocate codec context\n");
                return;
            }

            int ret = avcodec_parameters_to_context(codec_ctx_ptr_.get(), codecpar);
            if (ret < 0){
                std::print(stderr, "Decode failed to copy codec parameters to context\n");
                return;
            }

            codec_ctx_ptr_->thread_count = 0;               // auto threads
            codec_ctx_ptr_->thread_type = FF_THREAD_FRAME;  // frame parallel

            ret = avcodec_open2(codec_ctx_ptr_.get(), codec, nullptr);
            if (ret < 0){
                std::print(stderr, "Decode could not open codec\n");
                return;
            }

            ready_ = true;
        }

        ~Decode()
        {
            stop();
        }



        void run()
        {
            if (!ready_ || codec_ctx_ptr_ == nullptr) {
                std::print(stderr, "Decode not ready, run() skipped\n");
                return;
            }
            if (thread_.joinable()) {
                std::print(stderr, "Demux already running, run() skipped\n");
                return;
            }
            thread_ = std::jthread([this](const std::stop_token& st){
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
                if (pkt_opt == std::nullopt) {
                    break; // upstream closed
                }

                auto& pkt = *pkt_opt;

                const int ret_send = avcodec_send_packet(codec_ctx_ptr_.get(), pkt.get());
                if (ret_send < 0) {
                    // bad packet or decoder state; skip this packet
                    continue;
                }

                while (st.stop_requested() == false)
                {
                    frame_ptr_t frame(
                        av_frame_alloc()
                        , [](AVFrame* f){ if (f != nullptr) av_frame_free(&f); }
                    );
                    if (frame == nullptr) {
                        frame_queue_.close();
                        return;
                    }

                    const int ret_recv = avcodec_receive_frame(codec_ctx_ptr_.get(), frame.get());

                    if (ret_recv == AVERROR(EAGAIN) || ret_recv == AVERROR_EOF) {
                        break; // decoder drained
                    }
                    if (ret_recv < 0) {
                        std::print(stderr, "Decode error for current packet\n");
                        break;
                    }

                    if (frame_queue_.push(std::move(frame)) == false) {
                        frame_queue_.close(); // downstream closed
                        return;
                    }
                }
            }

            // flush delayed frames
            avcodec_send_packet(codec_ctx_ptr_.get(), nullptr);
            while (st.stop_requested() == false)
            {
                frame_ptr_t frame(
                    av_frame_alloc()
                    , [](AVFrame* f){ if (f != nullptr) av_frame_free(&f); }
                );
                if (frame == nullptr) {
                    break;
                }

                const int ret_recv = avcodec_receive_frame(codec_ctx_ptr_.get(), frame.get());
                if (ret_recv == AVERROR(EAGAIN) || ret_recv == AVERROR_EOF) {
                    break;
                }
                if (ret_recv < 0) {
                    std::print(stderr, "Decode error for current packet\n");
                    break;
                }

                if (frame_queue_.push(std::move(frame)) == false) {
                    break;
                }
            }

            frame_queue_.close();
        }



    };



} // namespace liteP