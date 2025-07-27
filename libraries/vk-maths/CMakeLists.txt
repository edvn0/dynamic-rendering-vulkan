cmake_minimum_required(VERSION 3.21)
project(VkMaths LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(DISABLE_EXCEPTIONS ON)

add_library(${PROJECT_NAME} STATIC src/aabb.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan glm::glm)

if(ENABLE_TESTING)
    find_package(doctest REQUIRED CONFIG)

    enable_testing()

    add_executable(test_vk_maths test/main.cpp)
    target_link_libraries(test_vk_maths VkMaths::VkMaths doctest::doctest)

    add_test(NAME test_vk_maths COMMAND test_vk_maths)
endif()

add_library(VkMaths::VkMaths ALIAS VkMaths)
