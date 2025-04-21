from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout

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
        self.requires("glfw/3.4",options={"with_x11": True, "with_wayland": False})

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

