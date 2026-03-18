from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout


class RSC_Tools(ConanFile):
    name = "RSC-Tools"
    version = "26.1.0"
    settings = "os", "arch", "compiler", "build_type"

    def configure(self):
        self.options["spdlog"].use_std_fmt = True

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires("cli11/2.6.0")
        self.requires("json-schema-validator/2.4.0")
        self.requires("nlohmann_json/3.12.0", force=True)
        self.requires("spdlog/1.17.0")
