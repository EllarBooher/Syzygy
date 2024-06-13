add_subdirectory("syzygy")

add_executable(Syzygy main.cpp)

target_link_libraries(Syzygy syzygy)