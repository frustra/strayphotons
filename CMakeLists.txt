# Absolute, bare-minimum version is CMake 3.13.
# Attempting to downgrade to anything less (Ubuntu 18.04 still ships with CMake 3.10.2)
# will result in CMake compile errors in the ext directory.
cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
project(sp VERSION 0.1)

set(SP_VERSION_MAJOR "${CMAKE_PROJECT_VERSION_MAJOR}")
set(SP_VERSION_MINOR "${CMAKE_PROJECT_VERSION_MINOR}")
set(SP_VERSION "${SP_VERSION_MAJOR}.${SP_VERSION_MINOR}")

option(SP_PACKAGE_RELEASE "Build packaged release" off)
option(SP_NO_MODEL_COMPILE "Disable the model compiler, which needs Node.js" off)
option(SP_BUILD_RUST "Build and link against rust modules" on)

if(CMAKE_BUILD_TYPE MATCHES Release)
	message(STATUS "sp target is Release")
	set(SP_DEBUG 0)
elseif(CMAKE_BUILD_TYPE MATCHES Debug)
	message(STATUS "sp target is Debug")
	set(SP_DEBUG 1)
else()
	message(STATUS "sp target is default, using Debug")
	set(CMAKE_BUILD_TYPE Debug)
	set(SP_DEBUG 1)
endif()

if(SP_PACKAGE_RELEASE)
	message(STATUS "Enabling build for packaged release")
endif()

# Tell cmake to export a compile_commands.json file
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Tell cmake we need C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

## Setup compiler flags for various build types
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

	set(CMAKE_CXX_FLAGS 				"/MP /wd4800 /wd4189 /EHsc")
	set(CMAKE_CXX_FLAGS_RELEASE 		"/MD /O2")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 	"/MDd /O2")
	set(CMAKE_CXX_FLAGS_DEBUG 			"/MDd /DEBUG /ZI /FC")

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

	set(CMAKE_CXX_FLAGS 				"-Wall -Werror -Werror=format-security")
	set(CMAKE_CXX_FLAGS_RELEASE 		"-O3")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 	"-g3 -ggdb3 -Og")
	set(CMAKE_CXX_FLAGS_DEBUG 			"-g3 -ggdb3 -O0")

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")

	set(CMAKE_CXX_FLAGS 				"-Wall -Werror -Wformat-security")
	set(CMAKE_CXX_FLAGS_RELEASE 		"-O3")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 	"-g3 -ggdb3 -Og")
	set(CMAKE_CXX_FLAGS_DEBUG 			"-g3 -ggdb3 -O0")

else()

	message(FATAL_ERROR "Unknown CXX compiler ${CMAKE_CXX_COMPILER_ID}")

endif()

set(PROJECT_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# Put RUNTIME and LIBRARY outputs into a folder where we can run them without installing
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})

find_package(OpenGL REQUIRED)

# This variable contains the component name used in calls to install() for the core stray photons
# installation
# Some external projects (like OpenVR and fmod) require it to be used in their install() calls
set(PROJECT_INSTALL sp)

# Root name for all libraries that will be part of this project
set(PROJECT_BASE_NAME strayphotons)

# The absolute bare minimum core of the engine. Contains Tecs and GameLogic modules.
# All other modules / libraries depend on this core
set(PROJECT_LIB ${PROJECT_BASE_NAME}-core)

# Core library for OpenGL-based graphics subsystem
# Note: this does not get linked into a static lib directly, instead
# it gets consumed by either the -opengl or -egl versions
set(PROJECT_GRAPHICS_CORE_LIB ${PROJECT_BASE_NAME}-graphics-core)

# Library for OpenGL-based graphics using OpenGL 4.0
set(PROJECT_GRAPHICS_OPENGL_LIB ${PROJECT_BASE_NAME}-graphics-opengl)

# Library for OpenGL-based graphics using EGL
set(PROJECT_GRAPHICS_EGL_LIB ${PROJECT_BASE_NAME}-graphics-egl)

set(PROJECT_EXE sp)
set(PROJECT_TEST_EXE sp-test)

# Core StrayPhotons lib. Contains all game code.
add_library(${PROJECT_LIB} STATIC)

# StrayPhotons graphics lib. Contains all graphics-related code.
# Note: this lib is an OBJECT lib, and doesn't actually produce a functional
# static library that can be linked into an executable.
# This target must first be linked against OpenGL or EGL
# to produce a static library.
# Link executables against either PROJECT_GRAPHICS_OPENGL_LIB or PROJECT_GRAPHICS_EGL_LIB
add_library(${PROJECT_GRAPHICS_CORE_LIB} OBJECT)

# Implementation of the graphics module that links against real OpenGL
add_library(${PROJECT_GRAPHICS_OPENGL_LIB} STATIC)

# Implementation of the graphics module that links against EGL
add_library(${PROJECT_GRAPHICS_EGL_LIB} STATIC)

# Exe Shim that loads StrayPhotons lib on desktop platforms
add_executable(${PROJECT_EXE})

# Exe Shim that loads StrayPhotons lib for CI testing purposes
add_executable(${PROJECT_TEST_EXE})

# Build all external dependencies
add_subdirectory(ext)

if((CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64") OR (CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64"))
	set(ARM ON)
else()
	set(ARM OFF)
endif()

## OpenVR is supported on Windows, Linux, and MacOS, but not on ARM
if(NOT ARM)
	if((WIN32 OR UNIX) OR APPLE)
		set(XRSYSTEM_OPENVR ON)
	endif()
endif()

# Tell CMake to convert all relative paths in subdirectories to absolute paths
cmake_policy(SET CMP0076 NEW)

target_include_directories(
	${PROJECT_LIB}
	PUBLIC
		src
)

add_subdirectory(src)

add_subdirectory(assets)

## COMPILE DEFINITIONS ##

if (SP_PACKAGE_RELEASE)
	target_compile_definitions(
		${PROJECT_LIB}
		PUBLIC
			PACKAGE_RELEASE
	)

	# No way around using file(GLOB) here without
	# manually listing every file to be included
	# in the assets.spdata file
	#
	# When adding entirely new assets, CMake will need
	# to be re-run
	set(_asset_filename assets.spdata)
	file(GLOB_RECURSE _glb_assets RELATIVE "${CMAKE_CURRENT_LIST_DIR}/assets" CONFIGURE_DEPENDS "assets/models/*.glb")
	file(GLOB_RECURSE _glb_assets_full "assets/models/*.glb")

	file(GLOB_RECURSE _shaders_assets_full CONFIGURE_DEPENDS "src/shaders/*")

	add_custom_command(
		COMMAND
			${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/src/shaders ${CMAKE_CURRENT_LIST_DIR}/assets/shaders
		COMMAND
			${CMAKE_COMMAND} -E tar c ${PROJECT_OUTPUT_DIR}/${_asset_filename} -- cache fonts logos scenes textures shaders ${_glb_assets}
		COMMAND
			${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_LIST_DIR}/assets/shaders
		OUTPUT
			${PROJECT_OUTPUT_DIR}/${_asset_filename}
		DEPENDS
			${_glb_assets_full}
			${_shaders_assets_full}
		WORKING_DIRECTORY
			${CMAKE_CURRENT_LIST_DIR}/assets
	)

	add_custom_target(assets_tar DEPENDS ${PROJECT_OUTPUT_DIR}/${_asset_filename})

	# Make the TAR generation depend on having up-to-date models
	add_dependencies(assets_tar models)

	# Make the TAR generation depend on having up-to-date models
	add_dependencies(assets_tar compiled-scenes)

	# Make the exe depend on having the tar
	add_dependencies(${PROJECT_EXE} assets_tar)
	add_dependencies(${PROJECT_TEST_EXE} assets_tar)

endif()

if (MSVC)
	target_compile_definitions(
		${PROJECT_LIB}
		PUBLIC
			_CRT_SECURE_NO_WARNINGS
			UNICODE
			_UNICODE
			NOMINMAX
			_USE_MATH_DEFINES
	)
endif()

if (SP_DEBUG)
	target_compile_definitions(
		${PROJECT_LIB}
		PUBLIC
			_DEBUG
	)
else()
	target_compile_definitions(
		${PROJECT_LIB}
		PUBLIC
			NDEBUG
	)

	include(CheckIPOSupported)
	check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
	if(ipo_supported)
		message(STATUS "IPO / LTO enabled")
		set_target_properties(
			${PROJECT_LIB}
			${PROJECT_EXE}
			${PROJECT_TEST_EXE}
			PROPERTIES
				INTERPROCEDURAL_OPTIMIZATION TRUE)
	else()
		message(STATUS "IPO / LTO not supported: <${ipo_error}>")
	endif()
endif()

if(SP_BUILD_RUST)
	add_subdirectory(rust)

	target_compile_definitions(
		${PROJECT_LIB}
		PRIVATE
			RUST_CXX
	)

	target_link_libraries(
		${PROJECT_LIB}
		PRIVATE
			sp-rust
	)
	
	if(ipo_supported)
		set_target_properties(sp-rust PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
	endif()
endif()

## END COMPILE DEFINITIONS ##

if (SP_PACKAGE_RELEASE)
	if (MSVC)
		set_target_properties(${PROJECT_EXE} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
		set_target_properties(${PROJECT_TEST_EXE} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
		set_target_properties(${PROJECT_LIB} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
	else()
		set_target_properties(${PROJECT_EXE} PROPERTIES INSTALL_RPATH "\$ORIGIN")
		set_target_properties(${PROJECT_EXE} PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
	endif()
endif()

set (_physx_libraries
	PhysX
	PhysXCharacterKinematic
	PhysXCommon
	PhysXCooking
	PhysXExtensions
	PhysXFoundation
	PhysXPvdSDK
)

target_compile_options(${PROJECT_LIB} PRIVATE ${SP_TARGET_OPTIONS})
target_compile_options(${PROJECT_EXE} PRIVATE ${SP_TARGET_OPTIONS})
target_compile_options(${PROJECT_TEST_EXE} PRIVATE ${SP_TARGET_OPTIONS})

target_link_libraries(
	${PROJECT_LIB}
	PUBLIC
		${_physx_libraries}
		GLEW
		vhacd
		ImGui
		microtar
		murmurhash
		robin_hood
		tinygltf
		Tecs
		glm
		cxxopts
		OpenGL::GL
		picojson
)

# The graphics core lib depends on the core StrayPhotons library,
# but should not depend on anything else
target_link_libraries(
	${PROJECT_GRAPHICS_CORE_LIB}	
	PUBLIC
		${PROJECT_LIB}
)

target_link_libraries(
	${PROJECT_GRAPHICS_OPENGL_LIB} 
	PUBLIC
		${PROJECT_GRAPHICS_CORE_LIB}
		glfw)

target_link_libraries(
	${PROJECT_GRAPHICS_EGL_LIB}
	PUBLIC
		${PROJECT_GRAPHICS_CORE_LIB}
		glfw-egl)

target_link_libraries(
	${PROJECT_EXE}
	PRIVATE
		${PROJECT_LIB}
		${PROJECT_GRAPHICS_OPENGL_LIB}
)

target_link_libraries(
	${PROJECT_TEST_EXE}
	PRIVATE
		${PROJECT_LIB}
		${PROJECT_GRAPHICS_EGL_LIB}
)

if(UNIX)
	target_link_libraries(
		${PROJECT_LIB}
		PRIVATE
			pthread
	)
endif()

if (NOT WIN32)
	target_link_libraries(
		${PROJECT_LIB}
		PRIVATE
			linenoise
	)
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
	if(${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS 8.0)
		message(FATAL_ERROR "Requires GCC >8.0")
	endif()

	target_link_libraries(
		${PROJECT_LIB}
		PRIVATE
			stdc++fs
	)
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT sp)

install(
	TARGETS
		${PROJECT_EXE}
		${PROJECT_TEST_EXE}
		${PROJECT_LIB}
		${PROJECT_GRAPHICS_EGL_LIB}
		${PROJECT_GRAPHICS_OPENGL_LIB}
		${_physx_libraries}
	RUNTIME DESTINATION
		bin
	ARCHIVE	DESTINATION
		bin
	COMPONENT
		${PROJECT_INSTALL}
)

install(
	DIRECTORY
		assets/
	DESTINATION
		assets
	COMPONENT
		${PROJECT_INSTALL}
)

install(
	DIRECTORY
		src/shaders/
	DESTINATION
		src/shaders
	COMPONENT
		${PROJECT_INSTALL}
)

################################################
# Build tests
################################################
add_subdirectory(tests)
