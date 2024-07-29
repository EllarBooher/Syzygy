message(STATUS "syzygy - begin")

add_library(
	syzygy
	STATIC 
	"source/syzygy/syzygy.cpp"
	"source/syzygy/engine.cpp"
	"source/syzygy/initializers.cpp"
	"source/syzygy/images.cpp"
	"source/syzygy/helpers.cpp"
	"source/syzygy/descriptors.cpp"
	"source/syzygy/pipelines.cpp"
	"source/syzygy/shaders.cpp"
	"source/syzygy/assets.cpp"
	"source/syzygy/buffers.cpp"
	"source/syzygy/ui/engineui.cpp"
	"source/syzygy/ui/pipelineui.cpp"
	"source/syzygy/ui/propertytable.cpp"
	"source/syzygy/geometryhelpers.cpp"
	"source/syzygy/shadowpass.cpp"
	"source/syzygy/deferred/deferred.cpp"
	"source/syzygy/deferred/gbuffer.cpp"
	"source/syzygy/debuglines.cpp"
	"source/syzygy/editor/editor.cpp"
	"source/syzygy/editor/window.cpp" 
	"source/syzygy/editor/graphicscontext.cpp"
	"source/syzygy/vulkanusage.cpp"
	"source/syzygy/editor/swapchain.cpp"
	"source/syzygy/editor/framebuffer.cpp"
    "source/syzygy/renderpass/renderpass.cpp"
	"source/syzygy/ui/widgets.cpp"
	"source/syzygy/core/scene.cpp"
    "source/syzygy/core/immediate.cpp"
    "source/syzygy/core/scenetexture.cpp" 
	"source/syzygy/core/input.cpp"
)

add_dependencies(syzygy Shaders)

target_include_directories(
	syzygy
	PRIVATE 
	"${CMAKE_CURRENT_SOURCE_DIR}/source"
)
target_include_directories(
	syzygy
	PUBLIC 
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_compile_definitions(SOURCE_DIR="${CMAKE_SOURCE_DIR}")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	# TODO: remove alongside exceptions in source
	target_compile_options(
		syzygy
		PUBLIC
		-Xclang -fcxx-exceptions
	)
endif()

target_compile_definitions(
	syzygy
	PUBLIC
    GLFW_INCLUDE_VULKAN
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_DEPTH_ZERO_TO_ONE
	GLM_FORCE_SIZE_T_LENGTH
    GLM_FORCE_RADIANS
	GLM_FORCE_EXPLICIT_CTOR
	VK_NO_PROTOTYPES
)

FetchContent_MakeAvailable(glm)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

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
	${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
# Backends
	${IMGUI_BACKENDS_DIR}/imgui_impl_glfw.cpp
	${IMGUI_BACKENDS_DIR}/imgui_impl_vulkan.cpp
 )
target_include_directories(
	imgui SYSTEM
	PUBLIC 
	${imgui_SOURCE_DIR} 
	${IMGUI_BACKENDS_DIR} 
)
target_include_directories(
	imgui
	PUBLIC
	"${CMAKE_SOURCE_DIR}/thirdparty/imgui" # Patch in imconfig.h alternative
)
target_link_libraries(
	imgui
	PRIVATE
	volk
	glfw
)
target_link_libraries(
	imgui
	PUBLIC
	glm
)

target_compile_definitions(
	imgui
	PUBLIC
	VK_NO_PROTOTYPES # We use volk so we are dynamically linking the vulkan loader
	IMGUI_USER_CONFIG="imguiconfig.h"
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

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_definitions(
		fastgltf
		PRIVATE
		_SILENCE_CXX20_U8PATH_DEPRECATION_WARNING
	)
endif()

FetchContent_MakeAvailable(fmt)
FetchContent_MakeAvailable(volk)

# When using spirv-reflect's CMakeLists, spirv-reflect.h could not be found (in VS2022) 
# unless configuring CMake twice after deleting the cache.
# Thus, the we include the source instead of fetching it and configure the target ourselves
set(SPIRV-REFLECT_SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/spirv-reflect")
add_library(spirv-reflect
	${SPIRV-REFLECT_SOURCE_DIR}/spirv_reflect.cpp
	${SPIRV-REFLECT_SOURCE_DIR}/spirv_reflect.h
)
target_include_directories(spirv-reflect PUBLIC ${SPIRV-REFLECT_SOURCE_DIR})

target_link_libraries(
	syzygy
	PUBLIC
	spirv-reflect
	glm
	VulkanMemoryAllocator
	glfw
	imgui
	implot
	vk-bootstrap
	fastgltf
	fmt::fmt
	volk
)

add_executable(Syzygy main.cpp)
target_link_libraries(Syzygy syzygy)