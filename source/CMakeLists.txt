add_subdirectory("vulkan-renderer")

add_executable(Renderer main.cpp)

target_link_libraries(Renderer vulkan-renderer)