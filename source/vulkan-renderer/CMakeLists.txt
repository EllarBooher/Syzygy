add_library(vulkan-renderer STATIC 
	"Source/engine.cpp"
	"Source/initializers.cpp"
	"Source/images.cpp"
	"Source/helpers.cpp"
)

target_include_directories(vulkan-renderer PUBLIC 
	"${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

add_compile_definitions(SOURCE_DIR=${CMAKE_SOURCE_DIR})

target_compile_definitions(
	vulkan-renderer
	PUBLIC
    GLFW_INCLUDE_VULKAN
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RADIANS
	VK_NO_PROTOTYPES
)

FetchContent_MakeAvailable(Vulkan-Headers)
FetchContent_MakeAvailable(glm)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

FetchContent_MakeAvailable(stb)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
message("STB ${stb_SOURCE_DIR}")

FetchContent_MakeAvailable(glfw)

FetchContent_MakeAvailable(imgui)
add_library(imgui
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})

FetchContent_MakeAvailable(vk-bootstrap)
FetchContent_MakeAvailable(fastgltf)
FetchContent_MakeAvailable(fmt)
FetchContent_MakeAvailable(volk)

target_link_libraries(
	vulkan-renderer 
	PUBLIC
	Vulkan-Headers
	glm
	VulkanMemoryAllocator
	stb
	glfw
	imgui
	vk-bootstrap
	fastgltf
	fmt::fmt
	volk
)