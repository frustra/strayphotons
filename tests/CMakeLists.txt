cmake_minimum_required(VERSION ${CMAKE_MIN_VERSION} FATAL_ERROR)
project(sp-test)

################################
# Test Configuration
################################

include_directories(${project_include_dirs} ${GOOGLETEST_DIR}/include)

################################
# Test targets
################################

foreach(test_type unit integration)
	set(test_exe sp-${test_type}-tests)

	file(GLOB_RECURSE test_sources ${CMAKE_CURRENT_SOURCE_DIR}/${test_type}/*.cc)
	list(APPEND test_sources ${GOOGLETEST_DIR}/src/gtest_main.cc)
	list(REMOVE_DUPLICATES test_sources)

	file(GLOB_RECURSE test_headers ${CMAKE_CURRENT_SOURCE_DIR}/${test_type}/*.hh)
	list(REMOVE_DUPLICATES test_headers)

	add_executable(${test_exe} ${test_sources} ${test_headers})
	target_link_libraries(${test_exe} ${PROJECT_LIB} gtest gtest_main)

	# C++11
	target_compile_features(${test_exe} PRIVATE cxx_range_for)
	# Output location
	set_target_properties(${test_exe} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})
	set_target_properties(${test_exe} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_OUTPUT_DIR}/Release)
	set_target_properties(${test_exe} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_OUTPUT_DIR}/Debug)

	# target to run the tests
	add_custom_target(
		${test_type}-tests
		COMMAND ${test_exe}
		DEPENDS ${test_exe}
	COMMENT "Run ${test_type} tests")

endforeach(test_type)
