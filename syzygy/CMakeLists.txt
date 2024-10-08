message(STATUS "syzygy - begin")

add_library(
	syzygy
	STATIC 
	"source/syzygy/syzygy.cpp"
	"source/syzygy/assets/assets.cpp"

	"source/syzygy/geometry/geometryhelpers.cpp"
	"source/syzygy/geometry/geometrytypes.cpp"
	"source/syzygy/geometry/geometrytests.cpp"
	"source/syzygy/geometry/transform.cpp"

	"source/syzygy/renderer/pipelines/debuglines.cpp"
	"source/syzygy/renderer/pipelines/deferred.cpp"

	"source/syzygy/renderer/pipelines.cpp"
	"source/syzygy/renderer/renderer.cpp"
	"source/syzygy/renderer/vulkanstructs.cpp"
	"source/syzygy/renderer/gbuffer.cpp"
	"source/syzygy/renderer/shadowpass.cpp"
	"source/syzygy/renderer/shaders.cpp"
	"source/syzygy/renderer/descriptors.cpp"
	"source/syzygy/renderer/buffers.cpp"
	"source/syzygy/renderer/image.cpp"
	"source/syzygy/renderer/imageview.cpp"
	"source/syzygy/renderer/imageoperations.cpp"
    "source/syzygy/renderer/rendercommands.cpp"
	"source/syzygy/renderer/scenetexture.cpp" 	
	"source/syzygy/renderer/scene.cpp"
	"source/syzygy/renderer/material.cpp"
	"source/syzygy/renderer/lights.cpp"

	"source/syzygy/ui/engineui.cpp"
	"source/syzygy/ui/pipelineui.cpp"
	"source/syzygy/ui/hud.cpp"
    "source/syzygy/ui/uiwindowscope.cpp"
	"source/syzygy/ui/propertytable.cpp"
	"source/syzygy/ui/statelesswidgets.cpp"
	"source/syzygy/ui/dockinglayout.cpp"
	"source/syzygy/ui/texturedisplay.cpp"
	"source/syzygy/ui/uiwidgets.cpp"

	"source/syzygy/editor/editor.cpp"
	"source/syzygy/editor/window.cpp" 
	"source/syzygy/editor/graphicscontext.cpp"
	"source/syzygy/editor/swapchain.cpp"
	"source/syzygy/editor/framebuffer.cpp"
	"source/syzygy/editor/uilayer.cpp"

	"source/syzygy/core/log.cpp"
    "source/syzygy/core/immediate.cpp"
	"source/syzygy/core/input.cpp"
	"source/syzygy/core/uuid.cpp"

	"source/syzygy/platform/vulkanusage.cpp"
	"source/syzygy/platform/windowsplatformutils.cpp"
	"source/syzygy/platform/filesystemutils.cpp"
	"source/syzygy/renderer/pipelines/skyview.cpp"
 )

add_dependencies(syzygy syzygy-shaders)

target_include_directories(
	syzygy
	PRIVATE 
		"${CMAKE_CURRENT_SOURCE_DIR}/source"
	PUBLIC 
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
# TODO: remove alongside exceptions in source
	target_compile_options(
		syzygy
		PRIVATE
			-Xclang -fcxx-exceptions
	)
endif()

target_compile_definitions(
	syzygy
	PRIVATE
		$<$<CONFIG:Debug>:SZG_DEBUG_BUILD>
		$<$<CONFIG:RelWithDebInfo>:SZG_DEBUG_BUILD>
		GLFW_INCLUDE_VULKAN
		GLM_ENABLE_EXPERIMENTAL
		GLM_FORCE_DEPTH_ZERO_TO_ONE
		GLM_FORCE_SIZE_T_LENGTH
		GLM_FORCE_RADIANS
		GLM_FORCE_EXPLICIT_CTOR
# Volk metaloader will load the methods for us
		VK_NO_PROTOTYPES
)

##### Dear ImGui #####

FetchContent_MakeAvailable(imgui)

add_library(
	imgui
	STATIC
		"${imgui_SOURCE_DIR}/imgui.cpp"
		"${imgui_SOURCE_DIR}/imgui_demo.cpp"
		"${imgui_SOURCE_DIR}/imgui_draw.cpp"
		"${imgui_SOURCE_DIR}/imgui_tables.cpp"
		"${imgui_SOURCE_DIR}/imgui_widgets.cpp"
		"${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
		"${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
		"${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
 )
target_include_directories(
	imgui 
	SYSTEM
	PUBLIC 
		"${imgui_SOURCE_DIR}"
		"${imgui_SOURCE_DIR}/backends"
)
target_include_directories(
	imgui
	PUBLIC
# Path to our imguiconfig.h
		"${CMAKE_SOURCE_DIR}/thirdparty/imgui"
)
target_compile_definitions(
	imgui
	PRIVATE
# We use volk so we are dynamically linking the vulkan loader
		VK_NO_PROTOTYPES 
	PUBLIC
		IMGUI_USER_CONFIG="imguiconfig.h"
)
target_link_libraries(
	imgui
	PRIVATE
# link for GLFW and Vulkan backends
		volk
		glfw
	INTERFACE
# Indirect dependency on glm due to glm types in imguiconfig
		glm
)

##### implot #####

FetchContent_MakeAvailable(implot)

add_library(
	implot
	STATIC
		"${implot_SOURCE_DIR}/implot.cpp"
		"${implot_SOURCE_DIR}/implot_items.cpp"
		"${implot_SOURCE_DIR}/implot_internal.h"
)
target_include_directories(
	implot 
	PUBLIC 
		"${implot_SOURCE_DIR}"
)
target_link_libraries(
	implot
	PRIVATE
		imgui
)

##### fastglTF #####

FetchContent_MakeAvailable(fastgltf)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_definitions(
		fastgltf
		PRIVATE
			_SILENCE_CXX20_U8PATH_DEPRECATION_WARNING
	)
endif()

##### spirv-reflect #####

# When using spirv-reflect's CMakeLists, spirv-reflect.h could not be found 
# (in VS2022) unless configuring CMake twice after deleting the cache.
# So we add it ourselves.
set(SPIRV-REFLECT_SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/spirv-reflect")
add_library(
	spirv-reflect
	STATIC
		"${SPIRV-REFLECT_SOURCE_DIR}/spirv_reflect.cpp"
		"${SPIRV-REFLECT_SOURCE_DIR}/spirv_reflect.h"
)
target_include_directories(
	spirv-reflect 
	PUBLIC 
		"${SPIRV-REFLECT_SOURCE_DIR}"
)

##### stb #####

# We could use FetchContent, but the stb repo is quite large and contains many 
# things we will not be using.

set(STB_SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/stb")
add_library(
	stb 
	INTERFACE
)
target_include_directories(
	stb 
	INTERFACE 
		"${STB_SOURCE_DIR}/include"
)
target_compile_definitions(
	syzygy
	PRIVATE
		STBI_MAX_DIMENSIONS=2048
)

###############

# These dependencies require no additional configuration

FetchContent_MakeAvailable(glm)
FetchContent_MakeAvailable(VulkanMemoryAllocator)
FetchContent_MakeAvailable(glfw)
FetchContent_MakeAvailable(vk-bootstrap)
FetchContent_MakeAvailable(volk)
FetchContent_MakeAvailable(spdlog)

target_link_libraries(
	syzygy
	PRIVATE
		spirv-reflect
		glm
		VulkanMemoryAllocator
		glfw
		imgui
		implot
		vk-bootstrap
		fastgltf
		volk
		stb
		spdlog::spdlog
)