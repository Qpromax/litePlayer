from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, cmake_layout

class MyLibConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = "ffmpeg/[>=8.0.0]", "sdl/[>=3.2.2]"
    generators = "CMakeDeps"

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.user_presets_path = '.'
        tc.generate()