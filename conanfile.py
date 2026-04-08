from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, cmake_layout

class MyLibConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"

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