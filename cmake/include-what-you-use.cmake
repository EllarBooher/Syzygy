option(IWYU_ENABLE "Enable IWYU - this impacts compilation time." OFF)

if (NOT IWYU_ENABLE)
	message(STATUS "iwyu NOT enabled")
	return()
endif()

find_program(
	IWYU_PATH
	NAMES "include-what-you-use" "iwyu"
	DOC "The path of include-what-you-use, ran alongside compilation."
)

if (IWYU_PATH)
	message(STATUS "iwyu enabled - using ${IWYU_PATH}")
	set_property(
		TARGET syzygy
		PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_PATH}
	)
else()
	message(WARNING "iwyu enabled - unable to find program, or no path is specified")
endif()