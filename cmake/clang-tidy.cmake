option(CLANG_TIDY_ENABLE "Enable clang-tidy - this impacts compilation time." OFF)

if (NOT CLANG_TIDY_ENABLE)
	message(STATUS "clang-tidy NOT enabled")
	return()
endif()

find_program(
	CLANG_TIDY_PATH
	NAMES 
		"clang-tidy"
	DOC 
		"The path of clang-tidy, ran alongside compilation. Linting rules are found in '.clang-tidy'."
)
find_program(
	CLANG_APPLY_REPLACEMENTS_PATH
	NAMES 
		"clang-apply-replacements"
	DOC 
		"The path of clang-apply-replacements, used to apply the fixes output by clang-tidy."
)

if (CLANG_TIDY_PATH)
	message(STATUS "clang-tidy enabled - using ${CLANG_TIDY_PATH}")

	set_property(
		TARGET 
			syzygy
		PROPERTY 
			CXX_CLANG_TIDY 
				"${CLANG_TIDY_PATH}"
	)

	set(FIX_DIR_RELATIVE "clang-tidy-fixes-dir")
	set(FIX_FULL_DIR "${CMAKE_BINARY_DIR}/syzygy/${FIX_DIR_RELATIVE}")
	message(STATUS "clang-tidy fixes output to ${FIX_FULL_DIR}")

	set_target_properties(
		syzygy
		PROPERTIES
			CXX_CLANG_TIDY
				"${CLANG_TIDY_PATH}"
			CXX_CLANG_TIDY_EXPORT_FIXES_DIR
				"clang-tidy-fixes-dir"
	)
	add_custom_target(
		clang-tidy-apply-fixes 
		COMMAND
			"${CLANG_APPLY_REPLACEMENTS_PATH}"
			"${FIX_FULL_DIR}"
		WORKING_DIRECTORY
			"${CMAKE_SOURCE_DIR}"
		COMMENT
			"clang-apply-replacements: Applying replacements in ${FIX_FULL_DIR}"
	)
	set_target_properties(
		clang-tidy-apply-fixes 
		PROPERTIES 
			EXCLUDE_FROM_ALL 
				true
	)
else()
	message(WARNING "clang-tidy enabled - unable to find program, or no path is specified")
endif()
