from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, cmake_layout

class MyLibConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"

    default_options = {
        "ffmpeg/*:disable_everything": True,
        "ffmpeg/*:avcodec": True,
        "ffmpeg/*:avformat": True,
        "ffmpeg/*:avutil": True,
        "ffmpeg/*:swresample": True,
        "ffmpeg/*:swscale": True,
    }

    def requirements(self):
        self.requires("ffmpeg/8.0.1")
        self.requires("sdl/3.2.20")
        self.requires("pulseaudio/17.0", override=True)

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.user_presets_path = '.'
        tc.generate()