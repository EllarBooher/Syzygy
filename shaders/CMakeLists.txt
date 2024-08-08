set(GLSL_VALIDATOR_NAME "glslangValidator")

unset(GLSL_VALIDATOR CACHE)

find_program(
    GLSL_VALIDATOR ${GLSL_VALIDATOR_NAME} 
    HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/
)
if(NOT GLSL_VALIDATOR)
    message(FATAL_ERROR "${GLSL_VALIDATOR_NAME} not found - cannot compile shaders.")
else()
    message(STATUS "${GLSL_VALIDATOR_NAME} found - using ${GLSL_VALIDATOR} for shader compilation")
endif()

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
)

foreach(GLSL_PATH ${GLSL_SOURCE_FILES})
  set(SPIRV_PATH "${GLSL_PATH}.spv")
  message(VERBOSE "Detected shader ${GLSL_PATH} - output will be ${SPIRV_PATH}")
  
  add_custom_command(
    OUTPUT ${SPIRV_PATH}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL_PATH} -o ${SPIRV_PATH}
    DEPENDS ${GLSL_PATH}
  )
  
  list(APPEND SPIRV_BINARY_FILES ${SPIRV_PATH})
endforeach()

source_group("Shaders GLSL" FILES ${GLSL_SOURCE_FILES})

add_custom_target(
    syzygy-shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    SOURCES ${GLSL_SOURCE_FILES}
)