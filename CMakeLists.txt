cmake_minimum_required(VERSION 4.0)
project(litePlayer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lc++")

find_package(FFMPEG REQUIRED)
find_package(SDL3 REQUIRED)
include_directories(${SDL3_INCLUDE_DIRS})
find_package(OpenGL REQUIRED)

add_executable(litePlayer main.cpp)


target_include_directories(litePlayer PRIVATE
        ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(litePlayer PRIVATE
        ${FFMPEG_LIBRARIES}
        ${SDL3_LIBRARIES}
        OpenGL::GL
)
