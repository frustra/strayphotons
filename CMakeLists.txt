project(sp)
set(CMAKE_MIN_VERSION 3.1)
cmake_minimum_required(VERSION ${CMAKE_MIN_VERSION} FATAL_ERROR)

set(SP_VERSION_MAJOR "0")
set(SP_VERSION_MINOR "1")
set(SP_VERSION "${SP_VERSION_MAJOR}.${SP_VERSION_MINOR}")

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

if (UNIX)
	add_definitions(-Wall)
	add_definitions(-D_DEBUG)
endif()

if (MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS /wd4800 /MP)
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

set(EXT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/ext)

find_package(OpenGL REQUIRED)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
set(GLFW_INSTALL OFF CACHE BOOL "")
add_subdirectory(${EXT_PATH}/glfw)

add_subdirectory(${EXT_PATH}/freetype2)

set(GOOGLETEST_DIR ${EXT_PATH}/googletest/googletest)
add_subdirectory(${EXT_PATH}/googletest)

add_subdirectory(${EXT_PATH}/boost)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${EXT_PATH}/physx/PhysXSDK/Include
	${CMAKE_CURRENT_SOURCE_DIR}/src
	${EXT_PATH}/glfw/include
	${EXT_PATH}/glew/include
	${EXT_PATH}/boostconfig/include
	${EXT_PATH}/glm
	${EXT_PATH}/freetype2/include
	${EXT_PATH}/tinygltfloader
	${EXT_PATH}/glomerate/include
	${EXT_PATH}/boost
	${EXT_PATH}/fmod/include/fmod
	${EXT_PATH}/fmod/include/fmodstudio
	${EXT_PATH}
)

add_definitions(-DGLEW_STATIC=1)
add_library(GLEW STATIC ${EXT_PATH}/glew/src/glew.c)

file(GLOB ImGui_sources ${EXT_PATH}/imgui/imgui.cpp ${EXT_PATH}/imgui/imgui_draw.cpp)
add_library(ImGui STATIC ${ImGui_sources})


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


# load PhysX
set (PHYSX_ENV DEBUG)

if (WIN32)
	set (PHYSX_ARCH x86)
else()
	set (PHYSX_ARCH x64)
endif()

set (PHYSX_LOADS
	"PhysX3Common${PHYSX_ENV}_${PHYSX_ARCH}"
	"PhysX3Cooking${PHYSX_ENV}_${PHYSX_ARCH}"
	"PhysX3Extensions${PHYSX_ENV}"
	"PhysX3${PHYSX_ENV}_${PHYSX_ARCH}"
)

foreach (PHYSX_LIB ${PHYSX_LOADS})
	message(STATUS ${PHYSX_LIB})
	find_library(FOUND_LIB_${PHYSX_LIB}
		NAMES ${PHYSX_LIB}
		PATHS ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lib/physx
	)
	LIST(APPEND PHYSX_LIBRARIES ${FOUND_LIB_${PHYSX_LIB}})
endforeach (PHYSX_LIB)

# Load fmod
if (MSVC)
	set (FMOD_LIBS "fmod" "fmodstudio")
	set (FMOD_LOADS )
	set (FMOD_ARCH "x86")

	# visual studio complains that it can't find the DLLs if
	# they're not in "build" dir... *sigh*
	foreach (lib ${FMOD_LIBS})
		FILE(
			COPY ${EXT_PATH}/fmod/lib/${FMOD_ARCH}/${lib}.dll
			DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/build)
		list(APPEND FMOD_LOADS ${lib}_vc)
	endforeach()
else ()
	set (FMOD_LOADS "fmod" "fmodstudio")
	set (FMOD_ARCH "x86_64")
endif()

foreach (FMOD_LIB ${FMOD_LOADS})
	message(STATUS ${FMOD_LIB})

	find_library(FOUND_LIB_${FMOD_LIB}
		NAMES ${FMOD_LIB}
		PATHS ${EXT_PATH}/fmod/lib/${FMOD_ARCH}
	)
	LIST(APPEND FMOD_LIBRARIES ${FOUND_LIB_${FMOD_LIB}})
endforeach (FMOD_LIB)

add_executable(${PROJECT_EXE} ${PROJECT_MAIN_CC})
target_link_libraries(${PROJECT_EXE} ${PROJECT_LIB})
target_link_libraries(${PROJECT_LIB} ${PHYSX_LIBRARIES} ${FMOD_LIBRARIES})

# C++11
target_compile_features(${PROJECT_LIB} PRIVATE cxx_range_for)
target_compile_features(${PROJECT_EXE} PRIVATE cxx_range_for)

if (UNIX)
	target_compile_options(${PROJECT_LIB} PRIVATE -Werror)
	target_compile_options(${PROJECT_EXE} PRIVATE -Werror)
endif()

set_target_properties(${PROJECT_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})
set_target_properties(${PROJECT_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_OUTPUT_DIR}/Release)
set_target_properties(${PROJECT_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_OUTPUT_DIR}/Debug)

set(BOOST_LIBS
	boost_system
	boost_filesystem
)

target_link_libraries(${PROJECT_LIB}
	glfw
	${GLFW_LIBRARIES}
	freetype
	${BOOST_LIBS}
)

# hacky way to ensure that the boost libs are built.
# Simply depending on their libs isn't enough since they are built
# outside of cmake but need to be triggered by cmake.
# I can't figure out how to get a CMake "imported" library
# to trigger a custom command or target (see boost CMakelists.txt)
foreach (boost_lib ${BOOST_LIBS})
	add_dependencies(${PROJECT_LIB} ${boost_lib}_target)
endforeach()

target_link_libraries(${PROJECT_EXE}
	${PROJECT_LIB}
	${OPENGL_LIBRARIES}
	GLEW
	glfw
	${GLFW_LIBRARIES}
	freetype
	ImGui
)

################################################
# Build tests
################################################
add_subdirectory(tests)
