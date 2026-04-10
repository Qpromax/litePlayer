extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

#include "SDL3/SDL.h"
#include "OpenGL/gl3.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

#include "liteP.h"

using packet_ptr_t = std::unique_ptr<AVPacket, void(*)(AVPacket*)>;
using frame_ptr_t  = std::unique_ptr<AVFrame, void(*)(AVFrame*)>;

std::string readFile(const char* path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

int main()
{
    const char* media_path = "../../example.mp4";

    liteP::TSDeque<packet_ptr_t> video_packet_queue(120);
    liteP::TSDeque<packet_ptr_t> audio_packet_queue(120);
    liteP::TSDeque<frame_ptr_t>  video_frame_queue(60);

    liteP::Demux demux(video_packet_queue, audio_packet_queue, media_path);

    const AVCodecParameters* video_codecpar = demux.video_codecpar();
    if (video_codecpar == nullptr) {
        std::cerr << "main: demux video codecpar is null\n";
        return -1;
    }
    const auto [video_w, video_h] = demux.video_size();
    const AVRational video_time_base = demux.video_time_base();

    liteP::Decode decode(video_packet_queue, video_frame_queue, video_codecpar);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL_Init failed\n";
        return -2;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow(
        "litePlayer",
        video_w > 0 ? video_w : 640,
        video_h > 0 ? video_h : 360,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow failed\n";
        SDL_Quit();
        return -3;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        std::cerr << "SDL_GL_CreateContext failed\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -4;
    }

    std::string vertsrc = readFile("../../shader/vertex.shader");
    std::string fragsrc = readFile("../../shader/fragment.shader");
    liteP::Renderer renderer(video_w, video_h, vertsrc.c_str(), fragsrc.c_str());
    if (!renderer.ok()) {
        std::cerr << "renderer init failed\n";
        SDL_GL_DestroyContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -5;
    }

    demux.run();
    decode.run();

    bool quit = false;
    bool clock_started = false;
    int64_t first_pts = AV_NOPTS_VALUE;
    auto wall_start = std::chrono::steady_clock::now();

    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                quit = true;
                break;
            }
        }

        auto frame_opt = video_frame_queue.pop_front();
        if (frame_opt == std::nullopt) {
            break;
        }

        auto& frame = *frame_opt;
        const int64_t pts = (frame->best_effort_timestamp != AV_NOPTS_VALUE)
                            ? frame->best_effort_timestamp
                            : frame->pts;

        // Use stream time_base + frame pts to pace rendering on wall clock.
        if (pts != AV_NOPTS_VALUE && video_time_base.num > 0 && video_time_base.den > 0) {
            if (!clock_started) {
                first_pts = pts;
                wall_start = std::chrono::steady_clock::now();
                clock_started = true;
            }
            const double sec = static_cast<double>(pts - first_pts) * av_q2d(video_time_base);
            if (sec > 0.0) {
                const auto target = wall_start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<double>(sec));
                const auto now = std::chrono::steady_clock::now();
                if (target > now) {
                    std::this_thread::sleep_until(target);
                }
            }
        }

        renderer.renderFrame(frame.get());
        SDL_GL_SwapWindow(window);
    }

    demux.stop();
    decode.stop();
    renderer.shutdown();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
