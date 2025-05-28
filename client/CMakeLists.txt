cmake_minimum_required(VERSION 3.21)
project(Client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED CONFIG)
find_package(imguizmo REQUIRED CONFIG)
find_package(yaml-cpp REQUIRED CONFIG)
find_package(glm REQUIRED CONFIG)

add_executable(Client
  src/main.cpp
  src/app_layer.cpp
)

target_link_libraries(Client PRIVATE
  DynamicRendering::Core
  DynamicRendering::Pipeline
  DynamicRendering::Renderer
  DynamicRendering::Window
  DynamicRendering::Scene
  DynamicRendering::Assets
  glfw
  imgui
  implot
  imguizmo::imguizmo
)

target_include_directories(Client PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)

if(MSVC)
  target_compile_options(Client PRIVATE /W4 /WX)
else()
  target_compile_options(Client PRIVATE -Wall -Wextra -Werror)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  target_compile_definitions(${PROJECT_NAME} PRIVATE IS_DEBUG)
endif()