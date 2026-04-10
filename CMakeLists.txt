cmake_minimum_required(VERSION 3.23)
project(litePlayer)

set(CMAKE_CXX_STANDARD 23)

find_package(ffmpeg)
find_package(SDL3)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

add_executable(litePlayer main.cpp
        liteP.h
        EXCEPT.h)

target_include_directories(litePlayer PRIVATE
        ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(litePlayer PRIVATE
        ffmpeg::ffmpeg
        sdl::sdl
        OpenGL::GL
        GLEW::GLEW
)
