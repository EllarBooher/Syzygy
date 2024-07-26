cmake_minimum_required(VERSION 3.28)
project(Syzygy)

include(cmake/dependencies.cmake)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory("shaders")
add_subdirectory("source")

include(cmake/include-what-you-use.cmake)
include(cmake/clang-format.cmake)
include(cmake/clang-tidy.cmake)