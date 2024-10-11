cmake_minimum_required(VERSION 3.28)

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

file(
    GLOB_RECURSE 
        GLSL_SOURCE_FILES
    CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/shaders/*.frag"
        "${PROJECT_SOURCE_DIR}/shaders/*.vert"
        "${PROJECT_SOURCE_DIR}/shaders/*.comp"
)

foreach(GLSL_PATH ${GLSL_SOURCE_FILES})
    cmake_path(
        GET
            GLSL_PATH
        PARENT_PATH
            SPIRV_BUILD_DIR
    )
    cmake_path(
        GET
            GLSL_PATH
        FILENAME
            SHADER_NAME
    )
    cmake_path(
        RELATIVE_PATH 
            SPIRV_BUILD_DIR
        BASE_DIRECTORY 
            "${PROJECT_SOURCE_DIR}/shaders"
    )
    
    cmake_path(SET DEPFILE_PATH "spirv/${SPIRV_BUILD_DIR}/${SHADER_NAME}.d")
    cmake_path(SET SPIRV_PATH "spirv/${SPIRV_BUILD_DIR}/${SHADER_NAME}.spv")

    message(VERBOSE "Detected shader ${GLSL_PATH} - output will be ${SPIRV_PATH}")

    # Ninja-Multi Config seems to require depfile output paths to be relative
    # This caused issues with trying to copy the shaders back out of the build folder
    #
    # I need to investigate if there is a proper way for this, but for now we just have a copy + byproduct
    # in order to get the shaders into the shaders root, useful since shaders are roughly
    # build config independent and the application is setup to expect them there.

    cmake_path(SET IN_SOURCE_PATH "${CMAKE_SOURCE_DIR}/shaders/${SPIRV_BUILD_DIR}/${SHADER_NAME}.spv")

    add_custom_command(
        OUTPUT "${SPIRV_PATH}"
        COMMAND "${GLSL_VALIDATOR}" -V "${GLSL_PATH}" -o "${SPIRV_PATH}" --depfile "${DEPFILE_PATH}" -g
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_BINARY_DIR}/${SPIRV_PATH}" "${IN_SOURCE_PATH}"
        BYPRODUCTS "${DEPFILE_PATH}" "${IN_SOURCE_PATH}"
        DEPENDS "${GLSL_PATH}"
        DEPFILE "${DEPFILE_PATH}"
        VERBATIM
    )

    list(APPEND SPIRV_BINARY_FILES "${SPIRV_PATH}")
endforeach()

source_group("Shaders GLSL" FILES ${GLSL_SOURCE_FILES})

add_custom_target(
    syzygy-shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    SOURCES ${GLSL_SOURCE_FILES}
)