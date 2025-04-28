cmake_minimum_required(VERSION 3.21)
project(VulkanApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(DISABLE_EXCEPTIONS ON)

function(dynamic_rendering_set_target_options target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)

    if(DISABLE_EXCEPTIONS)
      target_compile_definitions(${target} PRIVATE _HAS_EXCEPTIONS=0)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -Wno-unused-command-line-argument)

    if(DISABLE_EXCEPTIONS)
      target_compile_options(${target} PRIVATE -fno-exceptions)
    endif()
  endif()

  if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_definitions(${target} PRIVATE IS_DEBUG)
  endif()
endfunction()

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED CONFIG)
find_package(imguizmo REQUIRED CONFIG)
find_package(yaml-cpp REQUIRED CONFIG)
find_package(glm REQUIRED CONFIG)

add_subdirectory(third_party)

# === Core Library ===
add_library(VulkanApp_Core STATIC
  src/core/device.cpp
  src/core/allocator.cpp
  src/core/command_buffer.cpp
  src/core/gpu_buffer.cpp
  src/core/image.cpp
  src/core/input.cpp
  src/core/event_system.cpp
)

target_include_directories(VulkanApp_Core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/dynamic_rendering>
  ${CMAKE_SOURCE_DIR}/third_party/imgui/backends
  ${CMAKE_SOURCE_DIR}/third_party/imgui
  ${glfw3_INCLUDE_DIRS}
)

target_link_libraries(VulkanApp_Core PUBLIC
  Vulkan::Vulkan
  glm::glm
  vk-bootstrap::vk-bootstrap
  VulkanMemoryAllocator
)

target_precompile_headers(VulkanApp_Core PUBLIC include/dynamic_rendering/dynamic_rendering_pch.hpp)

# === Pipeline Library ===
add_library(VulkanApp_Pipeline STATIC
  src/pipeline/blueprint_configuration.cpp
  src/pipeline/blueprint_registry.cpp
  src/pipeline/shader.cpp
  src/pipeline/pipeline_factory.cpp
  src/pipeline/compute_pipeline_factory.cpp
  src/pipeline/compiled_pipeline.cpp
)

target_include_directories(VulkanApp_Pipeline PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/dynamic_rendering>
)

target_link_libraries(VulkanApp_Pipeline PUBLIC
  VulkanApp_Core
  yaml-cpp
  spirv-reflect-static
)

target_precompile_headers(VulkanApp_Pipeline REUSE_FROM VulkanApp_Core)

# === Renderer Library ===
add_library(VulkanApp_Renderer STATIC
  src/renderer/renderer.cpp
  src/renderer/layer.cpp
  src/renderer/camera.cpp
  src/renderer/material.cpp
  src/renderer/material_bindings.cpp
)

target_include_directories(VulkanApp_Renderer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/dynamic_rendering>

  # This is just for now, layers should be moved to client code
  ${CMAKE_SOURCE_DIR}/third_party/imgui
)

target_link_libraries(VulkanApp_Renderer PUBLIC
  VulkanApp_Core
  VulkanApp_Pipeline
)

target_precompile_headers(VulkanApp_Renderer REUSE_FROM VulkanApp_Core)

# === Window Library ===
add_library(VulkanApp_Window STATIC
  src/window/window.cpp
  src/window/swapchain.cpp
  src/window/gui_system.cpp
)

target_include_directories(VulkanApp_Window PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/dynamic_rendering>
)

target_link_libraries(VulkanApp_Window PUBLIC
  VulkanApp_Core
  glfw
  imgui
  implot
  imguizmo::imguizmo
)

target_precompile_headers(VulkanApp_Window REUSE_FROM VulkanApp_Core)

# === ALIAS Targets ===
add_library(VulkanApp::Core ALIAS VulkanApp_Core)
add_library(VulkanApp::Pipeline ALIAS VulkanApp_Pipeline)
add_library(VulkanApp::Renderer ALIAS VulkanApp_Renderer)
add_library(VulkanApp::Window ALIAS VulkanApp_Window)

# === Executable ===
add_executable(VulkanApp src/main.cpp)

target_link_libraries(VulkanApp PRIVATE
  VulkanApp::Core
  VulkanApp::Pipeline
  VulkanApp::Renderer
  VulkanApp::Window
)

target_include_directories(VulkanApp PRIVATE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_compile_definitions(VulkanApp PRIVATE
  GLM_FORCE_LEFT_HANDED
  GLM_FORCE_DEPTH_ZERO_TO_ONE
)

# PCH (reused from Core)
target_precompile_headers(VulkanApp REUSE_FROM VulkanApp_Core)

dynamic_rendering_set_target_options(VulkanApp_Core)
dynamic_rendering_set_target_options(VulkanApp_Pipeline)
dynamic_rendering_set_target_options(VulkanApp_Renderer)
dynamic_rendering_set_target_options(VulkanApp_Window)
dynamic_rendering_set_target_options(VulkanApp)

function(generate_shaders)
  add_custom_command(
    OUTPUT shaders_compiled
    COMMAND ${CMAKE_COMMAND} -E echo "Compiling shaders using Python script..."
    COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_SOURCE_DIR}/scripts python ${CMAKE_SOURCE_DIR}/scripts/compile_shaders.py
    --source ${CMAKE_SOURCE_DIR}/assets/shaders
    --output ${CMAKE_BINARY_DIR}/assets/shaders
    COMMAND ${CMAKE_COMMAND} -E touch shaders_compiled
    DEPENDS ${CMAKE_SOURCE_DIR}/assets/shaders
    COMMENT "Running shader compiler"
    VERBATIM
  )

  add_custom_target(compile_shaders_target ALL DEPENDS shaders_compiled)
  add_dependencies(${PROJECT_NAME} compile_shaders_target)
endfunction()

function(copy_blueprints)
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
  add_dependencies(${PROJECT_NAME} copy_blueprints_target)
endfunction()

# === Shader / Blueprint Tools ===
generate_shaders()
copy_blueprints()
