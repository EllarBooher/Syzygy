find_program(
	CLANG_FORMAT_PATH 
	NAMES "clang-format"
	DOC "The path of clang-format to use. Produces a utility-target that runs on the entirety of '/source'."
)

if (CLANG_FORMAT_PATH)
	set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/syzygy")

	file(
		GLOB_RECURSE
		ALL_CXX_SOURCE_FILES
		${SOURCE_DIR}/*.[chi]pp 
	)

	message(STATUS "clang-format - using ${CLANG_FORMAT_PATH}")
	message(VERBOSE "clang-format - formatting files matching ${SOURCE_DIR}/*.[chi]pp")
	
	add_custom_target(
		clang-format
		COMMAND ${CLANG_FORMAT_PATH} -i -style=file ${ALL_CXX_SOURCE_FILES}
		DEPENDS ${ALL_CXX_SOURCE_FILES}
	)
	set_target_properties(clang-format PROPERTIES EXCLUDE_FROM_ALL true)
else()
	message(STATUS "clang-format - NOT found, skipping")
endif()