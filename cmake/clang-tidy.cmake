option(CLANG_TIDY_ENABLE "Enable clang-tidy - this impacts compilation time." OFF)
set(CLANG_TIDY_OPTIONS CACHE STRING "Additional options to pass to clang-tidy, e.g. -fix")

if (NOT CLANG_TIDY_ENABLE)
	message(STATUS "clang-tidy NOT enabled")
	return()
endif()

find_program(
	CLANG_TIDY_PATH
	NAMES "clang-tidy"
	DOC "The path of clang-tidy, ran alongside compilation. Linting rules are found in '.clang-tidy'."
)

if (CLANG_TIDY_PATH)
	set(CLANG_TIDY_WITH_OPTIONS ${CLANG_TIDY_PATH} ${CLANG_TIDY_OPTIONS})
	message(STATUS "clang-tidy enabled - using ${CLANG_TIDY_WITH_OPTIONS}")

	set_property(
		TARGET syzygy
		PROPERTY CXX_CLANG_TIDY ${CLANG_TIDY_WITH_OPTIONS}
	)
else()
	message(WARNING "clang-tidy enabled - unable to find program, or no path is specified")
endif()
