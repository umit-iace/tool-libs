## bandwidth summary of the periodic frames, see comm/telemetry.h
#
# tool_libs_telemetry_summary(<target> <source>)
#
# compiles <source> (containing TELEMETRY_SUMMARY) with the settings of <target>
# without linking it, the build fails if the frames exceed the link and the
# summary is printed after each build of <target>
# cached, so it is known where the function is called, also when cross compiling
find_program(TOOL_LIBS_PYTHON NAMES python3 python)
function(tool_libs_telemetry_summary target source)
	set(obj ${target}-telemetry)
	add_library(${obj} OBJECT ${source})
	# only the compile settings (including those of the linked libraries), linking would
	# add the sources of interface libraries, e.g. startup code
	target_include_directories(${obj} PRIVATE $<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>)
	target_compile_definitions(${obj} PRIVATE $<TARGET_PROPERTY:${target},COMPILE_DEFINITIONS>)
	# own section for the summary
	target_compile_options(${obj} PRIVATE $<TARGET_PROPERTY:${target},COMPILE_OPTIONS> -fdata-sections)
	target_compile_features(${obj} PRIVATE cxx_std_20)
	add_dependencies(${target} ${obj})
	if (NOT TOOL_LIBS_PYTHON)
		message(WARNING "no python, the telemetry summary of ${target} is checked but not printed")
		return()
	endif()
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND ${TOOL_LIBS_PYTHON} ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../scripts/telemetry.py
			${CMAKE_OBJCOPY} "$<TARGET_OBJECTS:${obj}>"
		COMMAND_EXPAND_LISTS
		VERBATIM
	)
endfunction()
