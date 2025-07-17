cmake_minimum_required(VERSION 3.21)
project(VkUtil LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(DISABLE_EXCEPTIONS ON)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} INTERFACE Vulkan::Vulkan)

if(ENABLE_TESTING)
    find_package(doctest REQUIRED CONFIG)

    enable_testing()

    add_executable(test_vk_util test/main.cpp)
    target_link_libraries(test_vk_util VkUtil::VkUtil doctest::doctest)

    add_test(NAME test_vk_util COMMAND test_vk_util)
endif()

add_library(VkUtil::VkUtil ALIAS VkUtil)
