project(sp-test)

# macro from https://stackoverflow.com/questions/10113017
macro(configure_msvc_runtime)
	if(MSVC)
		# Default to statically-linked runtime.
		if("${MSVC_RUNTIME}" STREQUAL "")
			set(MSVC_RUNTIME "static")
		endif()

		# Set compiler options.
		set(variables
			CMAKE_C_FLAGS_DEBUG
			CMAKE_C_FLAGS_MINSIZEREL
			CMAKE_C_FLAGS_RELEASE
			CMAKE_C_FLAGS_RELWITHDEBINFO
			CMAKE_CXX_FLAGS_DEBUG
			CMAKE_CXX_FLAGS_MINSIZEREL
			CMAKE_CXX_FLAGS_RELEASE
			CMAKE_CXX_FLAGS_RELWITHDEBINFO
		)
		if(${MSVC_RUNTIME} STREQUAL "static")
			message(STATUS
		  	"MSVC -> forcing use of statically-linked runtime."
			)
			foreach(variable ${variables})
				if(${variable} MATCHES "/MD")
					string(REGEX REPLACE
						"/MD" "/MT" ${variable} "${${variable}}")
				endif()
			endforeach()
		else()
			message(STATUS
				"MSVC -> forcing use of dynamically-linked runtime."
			)
			foreach(variable ${variables})
				if(${variable} MATCHES "/MT")
					string(REGEX REPLACE
						"/MT" "/MD" ${variable} "${${variable}}")
				endif()
			endforeach()
		endif()
	endif()
endmacro()

################################
# Test targets
################################

# need to force test exe to use static c runtime otherwise there
# are errors when linking with gtest
set(MSVC_RUNTIME "static")
configure_msvc_runtime()

foreach(test_type unit integration)
	set(test_exe sp-${test_type}-tests)

	file(GLOB_RECURSE test_sources ${CMAKE_CURRENT_SOURCE_DIR}/${test_type}/*.cc)
	list(REMOVE_DUPLICATES test_sources)

	file(GLOB_RECURSE test_headers ${CMAKE_CURRENT_SOURCE_DIR}/${test_type}/*.hh)
	list(REMOVE_DUPLICATES test_headers)

	add_executable(${test_exe} ${test_sources} ${test_headers})
	target_link_libraries(${test_exe} ${PROJECT_LIB} gtest gtest_main)

	# target to run the tests
	add_custom_target(
		${test_type}-tests
		COMMAND ${test_exe}
		DEPENDS ${test_exe}
	COMMENT "Run ${test_type} tests")

endforeach(test_type)
