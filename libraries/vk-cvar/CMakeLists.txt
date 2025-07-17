cmake_minimum_required(VERSION 3.21)
project(VkCVar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(DISABLE_EXCEPTIONS ON)

add_library(${PROJECT_NAME} STATIC src/cvar_manager.cpp src/cvar_value.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_precompile_headers(VkCVar PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/vk-cvar/vk-cvar_pch.hpp)
target_link_libraries(VkCVar PRIVATE VkUtil::VkUtil)

if(ENABLE_TESTING)
    find_package(doctest REQUIRED CONFIG)

    enable_testing()

    add_executable(test_vkcvar test/main.cpp "test/cvar_manager_tests.cpp")
    target_link_libraries(test_vkcvar VkUtil::VkUtil VkCVar::VkCVar doctest::doctest)

    add_test(NAME test_vkcvar COMMAND test_vkcvar)
endif()

add_library(VkCVar::VkCVar ALIAS VkCVar)
