set(IWYU_PATH CACHE FILEPATH "The path of IWYU to use.")

if (NOT IWYU_ENABLE)
	message(STATUS "iwyu NOT enabled")
	return()
endif()

if (NOT IWYU_PATH)
	unset(IWYU_PATH CACHE)
	find_program(
		IWYU_PATH 
		NAMES include-what-you-use iwyu
	)
endif()

if (IWYU_PATH)
	message(STATUS "iwyu enabled - using ${IWYU_PATH}")
	set_property(
		TARGET vulkan-renderer 
		PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_PATH}
	)
else()
	message(WARNING "iwyu enabled - unable to find program, and no path is specified")
endif()