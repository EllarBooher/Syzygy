cmake_minimum_required(VERSION 3.28)
project(VulkanRenderer)

include(cmake/dependencies.cmake)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory("shaders")
add_subdirectory("source")

include(cmake/clang-tools.cmake)