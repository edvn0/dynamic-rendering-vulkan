cmake_minimum_required(VERSION 3.21)
project(VulkanApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(DISABLE_EXCEPTIONS ON)


find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED CONFIG)
find_package(imguizmo REQUIRED CONFIG)
find_package(yaml-cpp REQUIRED CONFIG)
find_package(glm REQUIRED CONFIG)

add_subdirectory(third_party)

add_executable(${PROJECT_NAME}
  src/main.cpp
  src/window.cpp
  src/swapchain.cpp
  src/gui_system.cpp
  src/command_buffer.cpp
  src/allocator.cpp
  src/pipeline/blueprint_configuration.cpp
  src/pipeline/blueprint_registry.cpp
  src/pipeline/shader.cpp
  src/pipeline/pipeline_factory.cpp
  src/image.cpp
  src/device.cpp
  src/gpu_buffer.cpp
  src/renderer.cpp
  src/pipeline/compute_pipeline_factory.cpp
  src/pipeline/compiled_pipeline.cpp
  src/layer.cpp
  src/camera.cpp
  src/material.cpp
  src/input.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  Vulkan::Vulkan
  glm::glm
  vk-bootstrap::vk-bootstrap
  glfw
  imgui
  imguizmo::imguizmo
  VulkanMemoryAllocator
  yaml-cpp
  spirv-reflect-static
)

target_include_directories(${PROJECT_NAME} PRIVATE src)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  target_compile_definitions(${PROJECT_NAME} PRIVATE IS_DEBUG)
endif()

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)

  if (DISABLE_EXCEPTIONS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _HAS_EXCEPTIONS=0)
  endif()
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror -Wno-unused-command-line-argument)

  if (DISABLE_EXCEPTIONS)
    target_compile_options(${PROJECT_NAME} PRIVATE -fno-exceptions)
  endif()
endif()

function(generate_shaders)
  set(shader_source_dir ${CMAKE_SOURCE_DIR}/assets/shaders)
  set(shader_binary_dir ${CMAKE_BINARY_DIR}/assets/shaders)

  file(GLOB_RECURSE glsl_shaders
    ${shader_source_dir}/*.vert
    ${shader_source_dir}/*.frag
    ${shader_source_dir}/*.comp
  )

  set(compiled_shaders)

  foreach(glsl_shader ${glsl_shaders})
    get_filename_component(file_name ${glsl_shader} NAME)
    set(spirv_shader ${shader_binary_dir}/${file_name}.spv)

    add_custom_command(
      OUTPUT ${spirv_shader}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${shader_binary_dir}
      COMMAND glslangValidator -V ${glsl_shader} -o ${spirv_shader} -gVS
      DEPENDS ${glsl_shader}
      JOB_POOL compile_shaders
      COMMENT "Compiling shader ${file_name}"
      VERBATIM
    )

    list(APPEND compiled_shaders ${spirv_shader})
  endforeach()

  add_custom_target(compile_shaders_target ALL DEPENDS ${compiled_shaders})
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

set_property(GLOBAL PROPERTY JOB_POOLS compile_shaders=8)

generate_shaders()
copy_blueprints()
