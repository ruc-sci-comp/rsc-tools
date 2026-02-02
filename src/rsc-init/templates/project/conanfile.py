from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout


class Project(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    name = "{{name}}"
    version = "0.0.1"

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires("cli11/2.6.0")
        self.requires("nlohmann_json/3.12.0")
