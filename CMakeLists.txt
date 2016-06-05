project(sp)
set(CMAKE_MIN_VERSION 3.1)
cmake_minimum_required(VERSION ${CMAKE_MIN_VERSION} FATAL_ERROR)

set(SP_VERSION_MAJOR "0")
set(SP_VERSION_MINOR "1")
set(SP_VERSION "${SP_VERSION_MAJOR}.${SP_VERSION_MINOR}")

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

if (UNIX)
	add_definitions(-Wall)
endif()

if (MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS /wd4800)
endif()

set(PROJECT_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${PROJECT_OUTPUT_DIR}/Release)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${PROJECT_OUTPUT_DIR}/Debug)

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

find_package(OpenGL REQUIRED)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
set(GLFW_INSTALL OFF CACHE BOOL "")
add_subdirectory(${EXT_PATH}/glfw)

add_subdirectory(${EXT_PATH}/freetype2)

set(GOOGLETEST_DIR ${EXT_PATH}/googletest/googletest)
add_subdirectory(${EXT_PATH}/googletest)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/src
	${EXT_PATH}/glfw/include
	${EXT_PATH}/glew/include
	${EXT_PATH}/boostconfig/include
	${EXT_PATH}/glm
	${EXT_PATH}/freetype2/include
	${EXT_PATH}/tinygltfloader
	${EXT_PATH}
)

add_definitions(-DGLEW_STATIC=1)
add_library(GLEW STATIC ${EXT_PATH}/glew/src/glew.c)

# split into lib and exe so tests can link against lib
set(PROJECT_LIB strayphotons)
set(PROJECT_EXE sp)
set(PROJECT_MAIN_CC ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Main.cc)

file(GLOB_RECURSE all_sources ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc)
list(REMOVE_DUPLICATES all_sources)
list(REMOVE_ITEM all_sources ${PROJECT_MAIN_CC})

file(GLOB_RECURSE all_headers ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hh)
list(REMOVE_DUPLICATES all_headers)

file(GLOB_RECURSE all_shaders
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.glsl
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.vert
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.tesc
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.tese
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.geom
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.frag
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.comp)
list(REMOVE_DUPLICATES all_shaders)

add_library(${PROJECT_LIB} ${all_sources} ${all_headers} ${all_shaders})
add_executable(${PROJECT_EXE} ${PROJECT_MAIN_CC})
target_link_libraries(${PROJECT_EXE} ${PROJECT_LIB})

# C++11
target_compile_features(${PROJECT_LIB} PRIVATE cxx_range_for)
target_compile_features(${PROJECT_EXE} PRIVATE cxx_range_for)

set_target_properties(${PROJECT_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})
set_target_properties(${PROJECT_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_OUTPUT_DIR}/Release)
set_target_properties(${PROJECT_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_OUTPUT_DIR}/Debug)

target_link_libraries(${PROJECT_LIB} glfw ${GLFW_LIBRARIES} freetype)
target_link_libraries(${PROJECT_EXE} ${PROJECT_LIB})
target_link_libraries(sp ${OPENGL_LIBRARIES} GLEW glfw ${GLFW_LIBRARIES} freetype)

################################################
# Build tests
################################################
add_subdirectory(tests)
