find_program(
	CLANG_FORMAT_PATH 
	NAMES 
		"clang-format"
	DOC 
		"The path of clang-format to use. Produces a utility-target that runs on the entirety of '/source'."
)

if (CLANG_FORMAT_PATH)
	set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/syzygy")

	file(
		GLOB_RECURSE
			ALL_CXX_SOURCE_FILES
		RELATIVE 
			${CMAKE_SOURCE_DIR}
		CONFIGURE_DEPENDS
			${SOURCE_DIR}/*.[chi]pp 
	)

	message(STATUS "clang-format - using ${CLANG_FORMAT_PATH}")
	message(STATUS "clang-format - formatting files matching ${SOURCE_DIR}/*.[chi]pp")

	add_custom_target(
		clang-format-fix
		COMMAND 
			${CLANG_FORMAT_PATH} 
			-i 
			${ALL_CXX_SOURCE_FILES}
			-style=file 
		WORKING_DIRECTORY 
			${CMAKE_SOURCE_DIR}
		COMMENT
			"clang-format: Formatting with ${CLANG_FORMAT_PATH}. Applying fixes in-place."
	)
	add_custom_target(
		clang-format-check
		COMMAND 
			${CLANG_FORMAT_PATH} 
			${ALL_CXX_SOURCE_FILES}
			-style=file 
			--dry-run
		WORKING_DIRECTORY 
			${CMAKE_SOURCE_DIR}
		COMMENT
			"clang-format: Formatting with ${CLANG_FORMAT_PATH}. Dry run only, printing suggested fixes."
	)
	set_target_properties(
		clang-format-fix 
		PROPERTIES 
			EXCLUDE_FROM_ALL 
				true
	)
else()
	message(STATUS "clang-format - NOT found, skipping")
endif()