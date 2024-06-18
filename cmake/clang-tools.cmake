set(CLANG_FORMAT_PATH CACHE FILEPATH "The path of clang-format to use.")

find_program(
	CLANG_FORMAT "clang-format"
	HINTS CLANG_FORMAT_PATH
)

if (CLANG_FORMAT)
	set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/source")

	file(
		GLOB_RECURSE
		ALL_CXX_SOURCE_FILES
		${SOURCE_DIR}/*.[chi]pp 
	)

	message(STATUS "clang-format - using ${CLANG_FORMAT}")
	message(VERBOSE "clang-format - formatting files matching ${SOURCE_DIR}/*.[chi]pp")
	
	add_custom_target(
		clang-format
		COMMAND ${CLANG_FORMAT} -i -style=file ${ALL_CXX_SOURCE_FILES}
		DEPENDS ${ALL_CXX_SOURCE_FILES}
	)
	set_target_properties(clang-format PROPERTIES EXCLUDE_FROM_ALL true)
else()
	message(STATUS "clang-format - NOT found, skipping")
endif()

set(CLANG_TIDY_PATH CACHE FILEPATH "The path of clang-tidy to use.")
set(CLANG_TIDY_OPTIONS CACHE STRING "Additional options to pass to clang-tidy, i.e. -fix")

if (CLANG_TIDY_ENABLE)
	find_program(
		CLANG_TIDY "clang-tidy"
		HINTS CLANG_TIDY_PATH
	)

	if (CLANG_TIDY)
		set(CLANG_TIDY_WITH_OPTIONS ${CLANG_TIDY} ${CLANG_TIDY_OPTIONS})

		message(STATUS "clang-tidy enabled - using ${CLANG_TIDY_WITH_OPTIONS}")
		set_property(
			TARGET syzygy
			PROPERTY CXX_CLANG_TIDY ${CLANG_TIDY_WITH_OPTIONS}
		)
	else()
		message(WARNING "clang-tidy enabled - unable to find program, and no path is specified")
	endif()
else()
	message(STATUS "clang-tidy NOT enabled")
endif()