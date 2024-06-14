set(IWYU_PATH CACHE FILEPATH "The path of IWYU to use.")

if (NOT IWYU_ENABLE)
	message(STATUS "iwyu NOT enabled")
	return()
endif()

find_program(
	IWYU 
	NAMES include-what-you-use iwyu
)

if (IWYU)
	message(STATUS "iwyu enabled - using ${IWYU}")
	set_property(
		TARGET syzygy
		PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU}
	)
else()
	message(WARNING "iwyu enabled - NOT found")
endif()