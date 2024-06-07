cmake_minimum_required(VERSION 3.28)
project(VulkanRenderer)

include(cmake/dependencies.cmake)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory("shaders")
add_subdirectory("source")

option(IWYU_ENABLE "Enable usage of include-what-you-use via CMake." OFF)
include(cmake/include-what-you-use.cmake)

include(cmake/clang-tools.cmake)