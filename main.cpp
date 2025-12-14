extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include "SDL3/SDL.h"
#include "OpenGL/gl3.h"

#include <iostream>
#include <memory>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <format>

#include "liteP.h"


std::string readFile(const char* path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file: " << path << std::endl;
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

int main() {
    // std::cout << std::filesystem::current_path() << std::endl;
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window =  SDL_CreateWindow("litePlayer", 400, 300, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    liteP::MP4 a;
    a.init("../../example.mp4");
    liteP::Renderer renderer;
    std::string vertsrc = readFile("../../shader/vertex.shader");
    std::string fragsrc = readFile("../../shader/fragment.shader");
    renderer.init(a.width, a.height, vertsrc.c_str(), fragsrc.c_str());
    liteP::TSDeque<AVFrame*> frame_queue;
    a.decode(frame_queue);
    while (auto opt = frame_queue.front_pop())
    {
        AVFrame* frame = *opt;
        renderer.renderFrame(frame);
        SDL_GL_SwapWindow(window);
        SDL_Delay(40);
        av_frame_free(&frame);
    }

    SDL_DestroyWindow(window);
    SDL_GL_DestroyContext(gl_context);
    SDL_Quit();

    return 0;
}


// void dem(liteP::TSDeque<int>& pkg)
// {
//     for (int i = 0; i < 1000; ++i)
//     {
//         pkg.push(i);
//         std::cout << "demux push " << i << std::endl;
//     }
//     pkg.close();
// }
//
// void dec(liteP::TSDeque<int>& pkg, liteP::TSDeque<std::string>& frame)
// {
//     while (auto a = pkg.front_pop())
//     {
//         std::cout << "decode " << a.value() << std::endl;
//         frame.push(std::format("pkg {}", a.value()-1000));
//     }
//     frame.close();
// }
//
// void ren(liteP::TSDeque<std::string>& frame)
// {
//     while (auto a = frame.front_pop())
//     {
//         std::cout << a.value() << std::endl;
//     }
// }
//
// int main()
// {
//     liteP::TSDeque<int> pkg(100);
//     liteP::TSDeque<std::string> frame(60);
//     std::jthread demux(dem, std::ref(pkg));
//     std::jthread decode(dec, std::ref(pkg), std::ref(frame));
//     std::jthread render(ren, std::ref(frame));
// }