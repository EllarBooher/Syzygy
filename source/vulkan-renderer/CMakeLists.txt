add_library(vulkan-renderer STATIC 
	"Source/engine.cpp"
	"Source/initializers.cpp"
	"Source/images.cpp"
	"Source/helpers.cpp"
	"source/descriptors.cpp"
	"source/pipelines.cpp"
	"source/shaders.cpp"
	"source/application.cpp"
	"source/assets.cpp"
	"source/buffers.cpp"
)

target_include_directories(vulkan-renderer PUBLIC 
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# quotes to support path with spaces
add_compile_definitions(SOURCE_DIR="${CMAKE_SOURCE_DIR}")

target_compile_definitions(
	vulkan-renderer
	PUBLIC
    GLFW_INCLUDE_VULKAN
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RADIANS
	GLM_FORCE_EXPLICIT_CTOR
	VK_NO_PROTOTYPES
)

FetchContent_MakeAvailable(glm)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

FetchContent_MakeAvailable(stb)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

FetchContent_MakeAvailable(glfw)

FetchContent_MakeAvailable(imgui)

# ImGui currently does not support CMake, so we create targets ourselves.
set(IMGUI_BACKENDS_DIR ${imgui_SOURCE_DIR}/backends)

add_library(imgui
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp

# Backends
	${IMGUI_BACKENDS_DIR}/imgui_impl_glfw.cpp
	${IMGUI_BACKENDS_DIR}/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${IMGUI_BACKENDS_DIR})
target_link_libraries(
	imgui
	PRIVATE
	volk
	glfw
)
target_compile_definitions(
	imgui
	PUBLIC
	VK_NO_PROTOTYPES # We use volk so we are dynamically linking the vulkan loader
)

FetchContent_MakeAvailable(implot)

add_library(implot
	${implot_SOURCE_DIR}/implot.cpp
	${implot_SOURCE_DIR}/implot_items.cpp
	${implot_SOURCE_DIR}/implot_internal.h
)

target_include_directories(implot PUBLIC ${implot_SOURCE_DIR})
target_link_libraries(
	implot
	PRIVATE
	imgui
)

FetchContent_MakeAvailable(vk-bootstrap)
FetchContent_MakeAvailable(fastgltf)
FetchContent_MakeAvailable(fmt)
FetchContent_MakeAvailable(volk)

set(SPIRV_REFLECT_EXECUTABLE OFF)
set(SPIRV_REFLECT_STATIC_LIB ON)
FetchContent_MakeAvailable(spirv-reflect)

target_link_libraries(
	vulkan-renderer 
	PUBLIC
	glm
	VulkanMemoryAllocator
	stb
	glfw
	imgui
	implot
	vk-bootstrap
	fastgltf
	fmt::fmt
	volk
	"spirv-reflect-static"
)