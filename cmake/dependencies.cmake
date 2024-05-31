include(FetchContent)
# Disquiet to see git clone progress
set(FETCHCONTENT_QUIET ON)

FetchContent_Declare(
	fmt
	GIT_REPOSITORY https://github.com/fmtlib/fmt.git
	GIT_TAG 10.2.1
	GIT_SHALLOW ON
	GIT_PROGRESS ON
)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
	glfw
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG 3.4
	GIT_SHALLOW ON
	GIT_PROGRESS ON
)

FetchContent_Declare(
	fastgltf
	GIT_REPOSITORY https://github.com/spnda/fastgltf.git
	GIT_TAG v0.7.1
	GIT_SHALLOW ON
	GIT_PROGRESS ON
)

FetchContent_Declare(
	glm
	GIT_REPOSITORY https://github.com/g-truc/glm.git
	GIT_TAG 1.0.1
	GIT_SHALLOW ON
	GIT_PROGRESS ON
)

FetchContent_Declare(
	imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui
	GIT_TAG v1.90.6-docking
	GIT_SHALLOW ON
	GIT_PROGRESS ON
)

FetchContent_Declare(
	implot
	GIT_REPOSITORY https://github.com/epezent/implot
	GIT_TAG v0.16
	GIT_SHALLOW ON
	GIT_PROGRESS ON
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
)

FetchContent_Declare(
	vk-bootstrap
	GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
	GIT_TAG v1.3.280
	GIT_SHALLOW ON
	GIT_PROGRESS ON
)

FetchContent_Declare(
	VulkanMemoryAllocator
	GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
	GIT_TAG v3.0.1
	GIT_SHALLOW ON
	GIT_PROGRESS ON
)

FetchContent_Declare(
	volk
	GIT_REPOSITORY https://github.com/zeux/volk.git
	GIT_TAG 1.3.270
	GIT_SHALLOW ON
	GIT_PROGRESS ON
)