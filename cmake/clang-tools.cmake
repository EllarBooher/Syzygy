set(CLANG_FORMAT_PATH CACHE FILEPATH "The path of clang-format to use.")

if (NOT CLANG_FORMAT_PATH)
	unset(CLANG_FORMAT_PATH)
	find_program(
		CLANG_FORMAT_PATH 
		"clang-format"
	)
endif()

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

set(CLANG_TIDY_PATH CACHE FILEPATH "The path of clang-tidy to use.")

if (CLANG_TIDY_ENABLE)
	if (NOT CLANG_TIDY_PATH)
		unset(CLANG_TIDY_PATH)
		find_program(
			CLANG_TIDY_PATH
			"clang-tidy"
		)
	endif()

	if (CLANG_TIDY_PATH)
		set(PATH_AND_OPTIONS ${CLANG_TIDY_PATH} -fix)

		message(STATUS "clang-tidy enabled - using ${PATH_AND_OPTIONS}")
		set_property(
			TARGET vulkan-renderer
			PROPERTY CXX_CLANG_TIDY ${PATH_AND_OPTIONS}
		)
	else()
		message(WARNING "clang-tidy enabled - unable to find program, and no path is specified")
	endif()
else()
	message(STATUS "clang-tidy NOT enabled")
endif()