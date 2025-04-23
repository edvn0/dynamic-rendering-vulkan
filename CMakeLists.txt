cmake_minimum_required(VERSION 3.21)
project(VulkanApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED CONFIG)
find_package(imguizmo REQUIRED CONFIG)
find_package(yaml-cpp REQUIRED CONFIG)

add_subdirectory(third_party)

add_executable(${PROJECT_NAME} src/main.cpp src/window.cpp src/swapchain.cpp src/gui_system.cpp src/command_buffer.cpp src/allocator.cpp
    src/pipeline/blueprint_configuration.cpp src/pipeline/blueprint_registry.cpp src/pipeline/shader.cpp src/pipeline/pipeline_factory.cpp
    src/image.cpp src/device.cpp src/gpu_buffer.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan vk-bootstrap::vk-bootstrap glfw imgui imguizmo::imguizmo VulkanMemoryAllocator yaml-cpp)
target_include_directories(${PROJECT_NAME} PRIVATE src)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)
endif()