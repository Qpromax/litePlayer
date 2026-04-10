cmake_minimum_required(VERSION 3.20)
project(litePlayer)

set(CMAKE_CXX_STANDARD 23)

find_package(ffmpeg)
find_package(SDL3)
find_package(opengl_system)

add_executable(litePlayer main.cpp
        liteP.h
        EXCEPT.h)

target_include_directories(litePlayer PRIVATE
        ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(litePlayer PRIVATE
        ffmpeg::ffmpeg
        sdl::sdl
        opengl::opengl
)
