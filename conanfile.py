import os

from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMake
from conan.tools.files import copy


class VulkanAppConan(ConanFile):
    name = "vulkan_app"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"
    requires = []
    tool_requires = []

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires("glfw/3.4")
        self.requires("imguizmo/1.83")
        self.requires("yaml-cpp/0.8.0")
        self.requires("glm/1.0.1")
        self.requires("efsw/1.4.1")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
