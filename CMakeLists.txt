project(sp)

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

set(SP_VERSION_MAJOR "0")
set(SP_VERSION_MINOR "1")
set(SP_VERSION "${SP_VERSION_MAJOR}.${SP_VERSION_MINOR}")

if (UNIX)
	add_definitions(-Wall)
endif()

if (MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

set(PROJECT_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)


if (WIN32)
	set(_SP_WIN32 1)
elseif (UNIX)
	set(_SP_X11 1)
else()
	message(FATAL_ERROR "Unsupported target platform")
endif()

if (CMAKE_BUILD_TYPE MATCHES Release)
	message(STATUS "sp target is Release")
elseif (CMAKE_BUILD_TYPE MATCHES Debug)
	message(STATUS "sp target is Debug")
else()
	message(STATUS "sp target is default, using Debug")
	set(CMAKE_BUILD_TYPE Debug)
endif()


set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${PROJECT_OUTPUT_DIR}/Release)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${PROJECT_OUTPUT_DIR}/Debug)

set(EXT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/ext)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs")
set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target")
add_subdirectory(${EXT_PATH}/glfw)

add_subdirectory(${EXT_PATH}/freetype2)

find_package(Boost 1.54 COMPONENTS locale REQUIRED)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${EXT_PATH}/glfw/include
	${EXT_PATH}/glm
	${EXT_PATH}/freetype2/include
	${Boost_INCLUDE_DIR}
)

file(GLOB_RECURSE all_sources ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc)
list(REMOVE_DUPLICATES all_sources)

file(GLOB_RECURSE all_headers ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hh)
list(REMOVE_DUPLICATES all_headers)

add_executable(sp ${all_sources} ${all_headers})

target_compile_features(sp PRIVATE cxx_range_for)
set_target_properties(sp PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})
set_target_properties(sp PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_OUTPUT_DIR}/Release)
set_target_properties(sp PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_OUTPUT_DIR}/Debug)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})

target_link_libraries(sp glfw ${GLFW_LIBRARIES} freetype)
