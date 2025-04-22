cmake_minimum_required(VERSION 3.21)
project(VulkanApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(imgui REQUIRED CONFIG)
find_package(glfw3 REQUIRED CONFIG)

add_subdirectory(third_party)

add_executable(${PROJECT_NAME} src/main.cpp bindings/imgui_impl_glfw.cpp bindings/imgui_impl_vulkan.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan vk-bootstrap::vk-bootstrap glfw imgui::imgui)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/bindings)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)
endif()