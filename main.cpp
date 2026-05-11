extern "C"
{
#include "libavcodec/packet.h"
#include "libavutil/frame.h"
}
#include <GLFW/glfw3.h>

#include <chrono>
#include <fstream>
#include <print>
#include <sstream>

#include "src/engine/decoder.h"
#include "src/engine/demuxer.h"
#include "src/engine/queue.h"
// #include "src/logic/executor.h"
#include "src/renderer/video.h"
#include "src/utils/ffmpeg_deleter.h"

using ptr_packet_t = std::unique_ptr<AVPacket, av_packet_deleter>;
using ptr_frame_t  = std::unique_ptr<AVFrame, av_frame_deleter>;

std::string read_file(const char* path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::print(stderr, "Failed to open file: {}", path);
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

int main(int argc, char* argv[])
{
    const char* media_path = "../../../../example.mp4";
    if (argc > 1)
    {
        media_path = argv[1];
    }

    std::print("{}\n", media_path);

    SPCQueue<ptr_packet_t> video_packet_queue;
    SPCQueue<ptr_packet_t> audio_packet_queue;
    SPCQueue<ptr_frame_t>  video_frame_queue;

    Demuxer demux(video_packet_queue, audio_packet_queue, media_path);

    const AVCodecParameters* video_codecpar = demux.video_codecpar();
    if (video_codecpar == nullptr)
    {
        std::print(stderr, "main: demux video codecpar is null\n");
        return -1;
    }
    const auto [video_w, video_h]    = demux.video_size();
    const AVRational video_time_base = demux.video_time_base();

    Decoder decode(video_packet_queue, video_frame_queue, video_codecpar);

    if (glfwInit() == GLFW_FALSE)
    {
        std::print(stderr, "glfwInit failed\n");
        return -2;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    GLFWwindow* window
        = glfwCreateWindow(video_w > 0 ? video_w : 640, video_h > 0 ? video_h : 360, "litePlayer", nullptr, nullptr);
    if (window == nullptr)
    {
        std::print(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return -3;
    }

    glfwMakeContextCurrent(window);
    if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0)
    {
        std::print(stderr, "gladLoadGLLoader failed\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return -4;
    }
    glfwSwapInterval(1);

    std::string vertsrc = read_file("../../../../shader/vertex.shader");
    std::string fragsrc = read_file("../../../../shader/fragment.shader");
    Renderer    renderer(video_w, video_h, vertsrc.c_str(), fragsrc.c_str());
    if (!renderer.ok())
    {
        std::print(stderr, "renderer init failed\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return -5;
    }

    int fbw = 0;
    int fbh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);

    demux.run();
    decode.run();

    bool    quit          = false;
    bool    clock_started = false;
    int64_t first_pts     = AV_NOPTS_VALUE;
    auto    wall_start    = std::chrono::steady_clock::now();

    while (!quit)
    {
        std::print("1");

        glfwPollEvents();
        if (glfwWindowShouldClose(window) == GLFW_TRUE)
        {
            quit = true;
            break;
        }

        auto frame_opt = video_frame_queue.pop();
        if (frame_opt == std::nullopt)
        {
            if (!video_frame_queue.running_status())
            {
                break;
            }
            continue;
        }

        auto&         frame = *frame_opt;
        const int64_t pts
            = (frame->best_effort_timestamp != AV_NOPTS_VALUE) ? frame->best_effort_timestamp : frame->pts;

        // Use stream time_base + frame pts to pace rendering on wall clock.
        if (pts != AV_NOPTS_VALUE && video_time_base.num > 0 && video_time_base.den > 0)
        {
            if (!clock_started)
            {
                first_pts     = pts;
                wall_start    = std::chrono::steady_clock::now();
                clock_started = true;
            }
            const double sec = static_cast<double>(pts - first_pts) * av_q2d(video_time_base);
            if (sec > 0.0)
            {
                const auto target = wall_start
                                  + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                        std::chrono::duration<double>(sec));
                const auto now    = std::chrono::steady_clock::now();
                if (target > now)
                {
                    std::this_thread::sleep_until(target);
                }
            }
        }

        glfwGetFramebufferSize(window, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);
        renderer.renderFrame(frame.get());
        glfwSwapBuffers(window);
    }

    demux.stop();
    decode.stop();
    renderer.shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
