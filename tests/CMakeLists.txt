cmake_minimum_required(VERSION ${CMAKE_MIN_VERSION} FATAL_ERROR)

set(PROJECT_TEST sp-test)
set(UNIT_TEST_EXE sp-unit-tests)
project(${PROJECT_TEST})

################################
# Test Configuration
################################

include_directories(${project_include_dirs} ${GOOGLETEST_DIR}/include)

file(GLOB_RECURSE test_sources ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)
list(APPEND test_sources ${GOOGLETEST_DIR}/src/gtest_main.cc)
list(REMOVE_DUPLICATES test_sources)

file(GLOB_RECURSE test_headers ${CMAKE_CURRENT_SOURCE_DIR}/*.hh)
list(REMOVE_DUPLICATES test_headers)

################################
# Unit Tests
################################
add_executable(${UNIT_TEST_EXE} ${test_sources} ${test_headers})
target_link_libraries(${UNIT_TEST_EXE} ${PROJECT_LIB} gtest gtest_main)

# C++11
target_compile_features(${UNIT_TEST_EXE} PRIVATE cxx_range_for)
# Output location
set_target_properties(${UNIT_TEST_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})
set_target_properties(${UNIT_TEST_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_OUTPUT_DIR}/Release)
set_target_properties(${UNIT_TEST_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_OUTPUT_DIR}/Debug)

# target to run the unit tests
add_custom_target(
	unit-tests
	COMMAND ${UNIT_TEST_EXE}
	DEPENDS ${UNIT_TEST_EXE}
	COMMENT "Run unit tests")
