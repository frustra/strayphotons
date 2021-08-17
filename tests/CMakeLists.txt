project(sp-test)

################################
# Test targets
################################

foreach(test_type unit integration)
	set(test_exe sp-${test_type}-tests)

	file(GLOB_RECURSE test_sources ${CMAKE_CURRENT_SOURCE_DIR}/${test_type}/*.cc)
	list(REMOVE_DUPLICATES test_sources)

	add_executable(${test_exe} tests.cc ${test_sources})
	target_compile_definitions(${test_exe} PRIVATE TEST_TYPE=\"${test_type}\")
	target_link_libraries(${test_exe} ${PROJECT_CORE_LIB} ${PROJECT_INPUT_CORE_LIB})
	target_include_directories(${test_exe} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

	# target to run the tests
	add_custom_target(
		${test_type}-tests
		COMMAND ${test_exe}
		DEPENDS ${test_exe}
	COMMENT "Run ${test_type} tests")

endforeach(test_type)
