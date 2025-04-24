cmake_minimum_required(VERSION 3.21)
project(VulkanApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED CONFIG)
find_package(imguizmo REQUIRED CONFIG)
find_package(yaml-cpp REQUIRED CONFIG)
find_package(glm REQUIRED CONFIG)

add_subdirectory(third_party)

add_executable(${PROJECT_NAME} src/main.cpp src/window.cpp src/swapchain.cpp src/gui_system.cpp src/command_buffer.cpp src/allocator.cpp
  src/pipeline/blueprint_configuration.cpp src/pipeline/blueprint_registry.cpp src/pipeline/shader.cpp src/pipeline/pipeline_factory.cpp
  src/image.cpp src/device.cpp src/gpu_buffer.cpp src/renderer.cpp src/pipeline/compute_pipeline_factory.cpp src/layer.cpp
  src/camera.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan glm::glm vk-bootstrap::vk-bootstrap glfw imgui imguizmo::imguizmo VulkanMemoryAllocator yaml-cpp)
target_include_directories(${PROJECT_NAME} PRIVATE src)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  target_compile_definitions(${PROJECT_NAME} PRIVATE IS_DEBUG)
endif()

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)
endif()

function(GENERATE_SHADERS)
  set(SHADER_SOURCE_DIR ${CMAKE_SOURCE_DIR}/assets/shaders)
  set(SHADER_BINARY_DIR ${CMAKE_BINARY_DIR}/assets/shaders)

  file(GLOB_RECURSE GLSL_SHADERS
    ${SHADER_SOURCE_DIR}/*.vert
    ${SHADER_SOURCE_DIR}/*.frag
    ${SHADER_SOURCE_DIR}/*.comp
  )

  set(COMPILED_SHADERS "")

  foreach(GLSL ${GLSL_SHADERS})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV ${SHADER_BINARY_DIR}/${FILE_NAME}.spv)
    add_custom_command(
      OUTPUT ${SPIRV}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BINARY_DIR}
      COMMAND glslangValidator -V ${GLSL} -o ${SPIRV} -gVS
      DEPENDS ${GLSL}
      COMMENT "Compiling shader ${FILE_NAME}"
      VERBATIM
    )
    list(APPEND COMPILED_SHADERS ${SPIRV})
  endforeach()

  add_custom_target(CompileShaders ALL DEPENDS ${COMPILED_SHADERS})
  add_dependencies(${PROJECT_NAME} CompileShaders)
endfunction(GENERATE_SHADERS)

function(COPY_BLUEPRINTS)
  set(BLUEPRINT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/assets/blueprints)
  set(BLUEPRINT_BINARY_DIR ${CMAKE_BINARY_DIR}/assets/blueprints)

  add_custom_command(
    OUTPUT ${BLUEPRINT_BINARY_DIR}/.copied_marker
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${BLUEPRINT_SOURCE_DIR} ${BLUEPRINT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E touch ${BLUEPRINT_BINARY_DIR}/.copied_marker
    DEPENDS ${BLUEPRINT_SOURCE_DIR}
    COMMENT "Copying blueprint YAMLs to output directory"
  )

  add_custom_target(CopyBlueprints ALL DEPENDS ${BLUEPRINT_BINARY_DIR}/.copied_marker)
  add_dependencies(${PROJECT_NAME} CopyBlueprints)
endfunction(COPY_BLUEPRINTS)

GENERATE_SHADERS()
COPY_BLUEPRINTS()
