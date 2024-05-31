cmake_minimum_required(VERSION 3.28)
project(VulkanRenderer)

include(cmake/dependencies.cmake)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory("source")

# Add a compile shaders target
# This requires glslangValidator to be installed such as via the Vulkan SDK.

set(GLSL_VALIDATOR_NAME "glslangValidator")
unset(GLSL_VALIDATOR CACHE)
find_program(GLSL_VALIDATOR ${GLSL_VALIDATOR_NAME} HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
cmake_path(GET GLSL_VALIDATOR FILENAME GLSL_FILENAME)
if(NOT ${GLSL_FILENAME} MATCHES "${GLSL_VALIDATOR_NAME}.exe")
    message(WARNING "GLSL validator not found. Searched for: '${GLSL_VALIDATOR_NAME}'. The Shaders target needed to compile new .spv files will not work.")
else()
    message(STATUS "Found GLSL validator, build Shaders target to generate updated .spv files.")
endif()

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
)
foreach(GLSL ${GLSL_SOURCE_FILES})
  set(SPIRV "${GLSL}.spv")
  message(STATUS "Detected shader ${GLSL}")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
)