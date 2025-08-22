from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps
from conan.tools.files import copy, rmdir
import os


class CppMiddleProjectSprint7(ConanFile):
    name = "cpp_middle_project_sprint_7"
    version = "1.0.0"
    settings = "os", "compiler", "build_type", "arch"
    default_options = {
        "boost/*:shared": False,
    }

    def requirements(self):
        self.requires("asio/1.34.2")
        self.requires("boost/1.88.0")
        self.requires("libiconv/1.18", override=True)
        self.requires("gtest/1.16.0")
        self.requires("spdlog/1.15.3")

    def layout(self):
        self.folders.source = "."
        self.folders.build = "build"
        self.folders.generators = "build/generators"

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
