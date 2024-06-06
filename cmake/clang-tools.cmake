find_program(CLANG_FORMAT_PATH "clang-format")
if (CLANG_FORMAT_PATH)
	set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/source")

	file(
		GLOB_RECURSE
		ALL_CXX_SOURCE_FILES
		${SOURCE_DIR}/*.[chi]pp 
	)	

	message(STATUS "clang-format found - using ${CLANG_FORMAT_PATH}")
	message(VERBOSE "formatting files matching ${SOURCE_DIR}/*.[chi]pp")
	
	add_custom_target(
		clang-format
		COMMAND ${CLANG_FORMAT_PATH} -i -style=file ${ALL_CXX_SOURCE_FILES}
		DEPENDS ${ALL_CXX_SOURCE_FILES}
	)
else()
	message(STATUS "clang-format not found")
endif()