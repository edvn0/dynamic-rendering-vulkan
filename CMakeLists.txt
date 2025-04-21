cmake_minimum_required(VERSION 3.21)
project(VulkanApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glfw REQUIRED)

include(FetchContent)
FetchContent_Declare(
    fetch_vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
    GIT_TAG        v1.4.312
)
FetchContent_MakeAvailable(fetch_vk_bootstrap)

add_executable(vulkan_app src/main.cpp)
target_link_libraries(vulkan_app PRIVATE Vulkan::Vulkan vk-bootstrap::vk-bootstrap GLFW::glfw)

