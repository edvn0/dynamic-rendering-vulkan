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
        self.requires(
            "spdlog/1.15.1", options={"use_std_fmt": True, "no_exceptions": True, "shared": False})
        self.requires("glfw/3.4")
        self.requires("imguizmo/1.83")
        self.requires("yaml-cpp/0.8.0")
        self.requires("glm/1.0.1")
        self.requires("ktx/4.3.2")
        self.requires("efsw/1.4.1")
        self.requires("entt/3.14.0")
        self.requires("lyra/1.6.1")
        self.requires("meshoptimizer/0.23")
        self.requires("tracy/0.11.1")
        self.requires("bshoshany-thread-pool/5.0.0")
        self.requires("assimp/5.4.3", options={"shared": False,
                                               "with_x": False,
                                               "with_3d": False,
                                               "with_ac": False,
                                               "with_3ds": False,
                                               "with_3mf": False,
                                               "with_amf": False,
                                               "with_ase": False,
                                               "with_b3d": False,
                                               "with_bvh": False,
                                               "with_cob": False,
                                               "with_csm": False,
                                               "with_dxf": False,
                                               "with_fbx": False,
                                               "with_hmp": False,
                                               "with_ifc": False,
                                               "with_iqm": False,
                                               "with_irr": False,
                                               "with_lwo": False,
                                               "with_lws": False,
                                               "with_m3d": False,
                                               "with_md2": False,
                                               "with_md3": False,
                                               "with_md5": False,
                                               "with_mdc": False,
                                               "with_mdl": False,
                                               "with_mmd": False,
                                               "with_ndo": False,
                                               "with_nff": False,
                                               "with_obj": False,
                                               "with_off": False,
                                               "with_ply": False,
                                               "with_q3d": False,
                                               "with_raw": False,
                                               "with_sib": False,
                                               "with_smd": False,
                                               "with_stl": False,
                                               "with_x3d": False,
                                               "with_xgl": False,
                                               "with_gltf": True,
                                               "with_ms3d": False,
                                               "with_ogre": False,
                                               "with_step": False,
                                               "with_blend": False,
                                               "with_q3bsp": False,
                                               "with_assbin": False,
                                               "with_collada": False,
                                               "with_irrmesh": False,
                                               "with_opengex": False,
                                               "with_terragen": False,
                                               "with_x_exporter": False,
                                               "double_precision": False,
                                               "with_3ds_exporter": False,
                                               "with_3mf_exporter": False,
                                               "with_fbx_exporter": False,
                                               "with_m3d_exporter": False,
                                               "with_obj_exporter": False,
                                               "with_ply_exporter": False,
                                               "with_stl_exporter": False,
                                               "with_x3d_exporter": False,
                                               "with_gltf_exporter": False,
                                               "with_pbrt_exporter": False,
                                               "with_step_exporter": False,
                                               "with_assbin_exporter": False,
                                               "with_assxml_exporter": False,
                                               "with_assjson_exporter": False,
                                               "with_collada_exporter": False,
                                               "with_opengex_exporter": False, })

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
