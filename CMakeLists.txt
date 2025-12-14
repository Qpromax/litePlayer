cmake_minimum_required(VERSION 4.0)
project(litePlayer)

set(CMAKE_CXX_STANDARD 26)

find_package(ffmpeg)
find_package(SDL3)
find_package(opengl_system)

add_executable(litePlayer main.cpp
        liteP.h)

target_include_directories(litePlayer PRIVATE
        ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(litePlayer PRIVATE
        ffmpeg::ffmpeg
        sdl::sdl
        opengl::opengl
)
