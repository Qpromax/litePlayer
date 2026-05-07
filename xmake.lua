set_project("litePlayer")

set_languages("c++23")

add_requires(
    "glfw",
    "glad",
    "ffmpeg",
    "stdexec",
    "imgui", {configs = {glfw_opengl3 = true}}
)

target("litePlayer")
    set_kind("binary")
    add_files("main.cpp")

    add_packages("glfw", "glad", "ffmpeg", "stdexec", "imgui")

    if is_plat("macosx") then
        add_defines("GL_SILENCE_DEPRECATION")

        add_frameworks(
            "Cocoa",
            "OpenGL",
            "IOKit",
            "CoreVideo",
            "AudioToolbox",
            "CoreAudio",
            "CoreMedia",
            "VideoToolbox",
            "CoreAudio",
            "AudioToolbox"
        )

    elseif is_plat("linux") then
        add_syslinks("pthread", "dl", "GL", "X11")
    end