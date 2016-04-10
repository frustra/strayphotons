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
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
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

################################################
# Build external projects
################################################

set(EXT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/ext)

# GLFW builds these things by default, we don't want them
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs")
set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target")
add_subdirectory(${EXT_PATH}/glfw)

add_subdirectory(${EXT_PATH}/freetype2)

set(SPIRV_SKIP_EXECUTABLES ON CACHE INTERNAL "Skip SPIRV executables")
add_subdirectory(${EXT_PATH}/SPIRV-Tools)

set(GOOGLETEST_DIR ${EXT_PATH}/googletest/googletest)
add_subdirectory(${EXT_PATH}/googletest)


################################################
# build shaders
################################################

file(GLOB_RECURSE all_shaders
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.vert
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.tesc
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.tese
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.geom
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.frag
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.comp)
list(REMOVE_DUPLICATES all_shaders)

find_host_package(PythonInterp)

add_custom_target(sp-shaders
	${PYTHON_EXECUTABLE}
	${CMAKE_CURRENT_SOURCE_DIR}/compile_shaders.py
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMENT "Compile shaders"
	SOURCES ${all_shaders})


################################################
# build project
################################################

set(PROJECT_LIB strayphotons)
set(PROJECT_EXE sp)

set(PROJECT_MAIN_CC ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Main.cc)

set(project_include_dirs
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${EXT_PATH}/glfw/include
	${EXT_PATH}/glm
	${EXT_PATH}/freetype2/include
	${EXT_PATH}/vkcpp
	${EXT_PATH}/SPIRV-Tools/include
	${VULKAN_INCLUDE_DIR}
	${EXT_PATH}
)
include_directories(${project_include_dirs})

file(GLOB_RECURSE lib_sources ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc)
list(REMOVE_DUPLICATES lib_sources)
list(REMOVE_ITEM lib_sources ${PROJECT_MAIN_CC})

file(GLOB_RECURSE all_headers ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hh)
list(REMOVE_DUPLICATES all_headers)

add_library(${PROJECT_LIB} ${lib_sources} ${all_headers})
add_executable(${PROJECT_EXE} ${PROJECT_MAIN_CC})

# C++11
target_compile_features(${PROJECT_LIB} PRIVATE cxx_range_for)
target_compile_features(${PROJECT_EXE} PRIVATE cxx_range_for)

set_target_properties(${PROJECT_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})
set_target_properties(${PROJECT_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_OUTPUT_DIR}/Release)
set_target_properties(${PROJECT_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_OUTPUT_DIR}/Debug)

add_dependencies(${PROJECT_LIB} sp-shaders)

target_link_libraries(${PROJECT_LIB} glfw ${GLFW_LIBRARIES} ${VULKAN_LIBRARY} freetype SPIRV-Tools)
target_link_libraries(${PROJECT_EXE} ${PROJECT_LIB})


################################################
# Build tests
################################################
add_subdirectory(tests)