cmake_minimum_required(VERSION 3.21)
project(DynamicRendering LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(DISABLE_EXCEPTIONS ON)

function(dynamic_rendering_set_target_options target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)

        if (DISABLE_EXCEPTIONS)
            target_compile_definitions(${target} PRIVATE _HAS_EXCEPTIONS=0)
        endif ()
    else ()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -Wno-unused-command-line-argument)

        if (DISABLE_EXCEPTIONS)
            target_compile_options(${target} PRIVATE -fno-exceptions)
        endif ()
    endif ()

    if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        target_compile_definitions(${target} PRIVATE IS_DEBUG)
    endif ()

    target_compile_definitions(${target} PRIVATE
            GLM_FORCE_LEFT_HANDED
            GLM_FORCE_DEPTH_ZERO_TO_ONE
    )
endfunction()

find_package(Python COMPONENTS Interpreter REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED CONFIG)
find_package(imguizmo REQUIRED CONFIG)
find_package(yaml-cpp REQUIRED CONFIG)
find_package(glm REQUIRED CONFIG)
find_package(efsw REQUIRED CONFIG)
find_package(lyra REQUIRED CONFIG)
find_package(Tracy REQUIRED CONFIG)
find_package(bshoshany-thread-pool REQUIRED CONFIG)
find_package(assimp REQUIRED CONFIG)

add_subdirectory(third_party)
add_subdirectory(client)

# === Core Library ===
add_library(DynamicRendering_Core STATIC
        src/core/device.cpp
        src/core/allocator.cpp
        src/core/command_buffer.cpp
        src/core/gpu_buffer.cpp
        src/core/image.cpp
        src/core/input.cpp
        src/core/event_system.cpp
        src/core/app.cpp
        src/core/asset_file_watcher.cpp
        src/core/asset_reloader.cpp
        src/core/fs.cpp
        src/core/debug_utils.cpp
)

target_include_directories(DynamicRendering_Core PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/dynamic_rendering>
        ${CMAKE_SOURCE_DIR}/third_party/imgui/backends
        ${CMAKE_SOURCE_DIR}/third_party/imgui
        ${glfw3_INCLUDE_DIRS}
)

target_link_libraries(DynamicRendering_Core PUBLIC
        Vulkan::Vulkan
        glm::glm
        vk-bootstrap::vk-bootstrap
        VulkanMemoryAllocator
        Tracy::TracyClient
        bshoshany-thread-pool::bshoshany-thread-pool
        PRIVATE
        implot
        efsw::efsw
        bfg::lyra
        stb::stb
)

target_precompile_headers(DynamicRendering_Core PUBLIC include/dynamic_rendering/dynamic_rendering_pch.hpp)

add_library(DynamicRendering_Pipeline STATIC
        src/pipeline/blueprint_configuration.cpp
        src/pipeline/blueprint_registry.cpp
        src/pipeline/shader.cpp
        src/pipeline/pipeline_factory.cpp
        src/pipeline/compute_pipeline_factory.cpp
        src/pipeline/compiled_pipeline.cpp
)

target_include_directories(DynamicRendering_Pipeline PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/dynamic_rendering>
)

target_link_libraries(DynamicRendering_Pipeline PUBLIC
        DynamicRendering_Core
        yaml-cpp::yaml-cpp
)

target_precompile_headers(DynamicRendering_Pipeline REUSE_FROM DynamicRendering_Core)

add_library(DynamicRendering_Renderer STATIC
        src/renderer/renderer.cpp
        src/renderer/camera.cpp
        src/renderer/material.cpp
        src/renderer/material_bindings.cpp
        src/renderer/descriptor_manager.cpp
        src/renderer/draw_list_manager.cpp
        src/renderer/draw_command.cpp
        src/renderer/mesh.cpp
)

target_include_directories(DynamicRendering_Renderer PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/dynamic_rendering>
)

target_link_libraries(DynamicRendering_Renderer PUBLIC
        DynamicRendering_Core
        DynamicRendering_Pipeline
        PRIVATE
        VulkanMemoryAllocator
        spirv-reflect-static
        assimp::assimp
)

target_precompile_headers(DynamicRendering_Renderer REUSE_FROM DynamicRendering_Core)

# === Window Library ===
add_library(DynamicRendering_Window STATIC
        src/window/window.cpp
        src/window/swapchain.cpp
        src/window/gui_system.cpp
        src/window/window_configuration.cpp
)

target_include_directories(DynamicRendering_Window PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/dynamic_rendering>
)

target_link_libraries(DynamicRendering_Window PUBLIC
        DynamicRendering_Core
        PRIVATE
        glfw imgui implot imguizmo::imguizmo yaml-cpp::yaml-cpp
        VulkanMemoryAllocator
)

target_precompile_headers(DynamicRendering_Window REUSE_FROM DynamicRendering_Core)

# === ALIAS Targets ===
add_library(DynamicRendering::Core ALIAS DynamicRendering_Core)
add_library(DynamicRendering::Pipeline ALIAS DynamicRendering_Pipeline)
add_library(DynamicRendering::Renderer ALIAS DynamicRendering_Renderer)
add_library(DynamicRendering::Window ALIAS DynamicRendering_Window)

dynamic_rendering_set_target_options(DynamicRendering_Core)
dynamic_rendering_set_target_options(DynamicRendering_Pipeline)
dynamic_rendering_set_target_options(DynamicRendering_Renderer)
dynamic_rendering_set_target_options(DynamicRendering_Window)

function(generate_shaders target)
    add_custom_command(
            OUTPUT shaders_compiled
            COMMAND ${CMAKE_COMMAND} -E echo "Compiling shaders using Python script..."
            COMMAND ${Python_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/compile_shaders.py
            --source ${CMAKE_SOURCE_DIR}/assets/shaders
            --output ${CMAKE_BINARY_DIR}/assets/shaders
            COMMAND ${CMAKE_COMMAND} -E touch shaders_compiled
            DEPENDS ${CMAKE_SOURCE_DIR}/assets/shaders
            COMMENT "Running shader compiler"
            VERBATIM
    )

    add_custom_target(compile_shaders_target ALL DEPENDS shaders_compiled)
    add_dependencies(${target} compile_shaders_target)
endfunction()

function(copy_blueprints target)
    set(blueprint_source_dir ${CMAKE_SOURCE_DIR}/assets/blueprints)
    set(blueprint_binary_dir ${CMAKE_BINARY_DIR}/assets/blueprints)

    file(GLOB_RECURSE blueprint_files ${blueprint_source_dir}/*)

    add_custom_command(
            OUTPUT ${blueprint_binary_dir}
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${blueprint_source_dir} ${blueprint_binary_dir}
            DEPENDS ${blueprint_files}
            COMMENT "Copying blueprint YAMLs to output directory"
            VERBATIM
    )

    add_custom_target(copy_blueprints_target ALL DEPENDS ${blueprint_binary_dir})
    add_dependencies(${target} copy_blueprints_target)
endfunction()

# === Shader / Blueprint Tools ===
generate_shaders(DynamicRendering_Core)
copy_blueprints(DynamicRendering_Core)
