SET(RENDERER_NAME "vulkan-renderer")
message(STATUS "${RENDERER_NAME} - begin")

add_library(${RENDERER_NAME} STATIC 
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
	"source/ui/engineui.cpp"
	"source/ui/pipelineui.cpp"
	"source/ui/propertytable.cpp"
	"source/geometryhelpers.cpp"
	"source/shadowpass.cpp"
	"source/deferred/deferred.cpp"
	"source/deferred/gbuffer.cpp"
	"source/debuglines.cpp"
)

target_include_directories(vulkan-renderer PUBLIC 
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
)

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
	vulkan-renderer 
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