
# Absolute, bare-minimum version is CMake 3.13.
# Attempting to downgrade to anything less (Ubuntu 18.04 still ships with CMake 3.10.2)
# will result in CMake compile errors in the ext directory.
cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
project(sp VERSION 0.1)

set(SP_VERSION_MAJOR "${CMAKE_PROJECT_VERSION_MAJOR}")
set(SP_VERSION_MINOR "${CMAKE_PROJECT_VERSION_MINOR}")
set(SP_VERSION "${SP_VERSION_MAJOR}.${SP_VERSION_MINOR}")

option(SP_PACKAGE_RELEASE "Build packaged release" off)

if (CMAKE_BUILD_TYPE MATCHES Release)
	message(STATUS "sp target is Release")
	set(SP_DEBUG 0)
elseif (CMAKE_BUILD_TYPE MATCHES Debug)
	message(STATUS "sp target is Debug")
	set(SP_DEBUG 1)
else()
	message(STATUS "sp target is default, using Debug")
	set(CMAKE_BUILD_TYPE Debug)
	set(SP_DEBUG 1)
endif()

if (SP_PACKAGE_RELEASE)
	message(STATUS "Enabling build for packaged release")
endif()

# Tell cmake to export a compile_commands.json file
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Tell cmake we need C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

## Setup compiler flags for various build types
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

	set(CMAKE_CXX_FLAGS 				"/MP /wd4800 /wd4189 /EHsc")
	set(CMAKE_CXX_FLAGS_RELEASE 		"/MD")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 	"/MDd")
	set(CMAKE_CXX_FLAGS_DEBUG 			"/MDd /DEBUG /ZI /FC")

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

	set(CMAKE_CXX_FLAGS 				"-fpermissive -Wall -Wno-unused-local-typedefs -Werror -Wno-error=sign-compare -Wno-error=restrict -Wno-error=catch-value= -Wno-error=format-security -Wno-error=strict-aliasing -Wno-error=parentheses -Wno-error=class-memaccess -Wno-error=int-to-pointer-cast -Wno-error=unused-but-set-variable -Wno-error=unused-result -Wno-error=maybe-uninitialized")
	set(CMAKE_CXX_FLAGS_RELEASE 		"-O3")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 	"-g3 -ggdb3 -Og")
	set(CMAKE_CXX_FLAGS_DEBUG 			"-g3 -ggdb3 -O0")

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")

	set(CMAKE_CXX_FLAGS 				"-fpermissive -Wall -Wno-unused-local-typedefs -Werror -Wno-sign-compare -Wno-unused-private-field -Wno-delete-non-abstract-non-virtual-dtor -Wno-int-to-void-pointer-cast")
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

# split into lib and exe so tests can link against lib
set(PROJECT_LIB strayphotons)
set(PROJECT_EXE sp)

# Core StrayPhotons lib. Contains all game code.
add_library(${PROJECT_LIB} STATIC)

# Exe Shim that loads StrayPhotons lib on desktop platforms
add_executable(${PROJECT_EXE})

# Build all external dependencies
add_subdirectory(ext)

if((CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64") OR (CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64"))
	set(ARM ON)
else()
	set(ARM OFF)
endif()

if(NOT ARM)
	set(SP_AUDIO ON)
else()
	set(SP_AUDIO OFF)
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
	PRIVATE
		src
)

target_include_directories(
	${PROJECT_EXE}
	PRIVATE
		src
)

add_subdirectory(src)

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
	file(GLOB_RECURSE _gltf_assets RELATIVE "${CMAKE_CURRENT_LIST_DIR}/assets" CONFIGURE_DEPENDS "assets/models/*.gltf")
	file(GLOB_RECURSE _gltf_assets_full "assets/models/*.gltf")

	file(GLOB_RECURSE _bin_assets RELATIVE "${CMAKE_CURRENT_LIST_DIR}/assets" CONFIGURE_DEPENDS "assets/models/*.bin")
	file(GLOB_RECURSE _bin_assets_full "assets/models/*.bin")

	file(GLOB_RECURSE _png_assets RELATIVE "${CMAKE_CURRENT_LIST_DIR}/assets" CONFIGURE_DEPENDS "assets/models/*.png")
	file(GLOB_RECURSE _png_assets_full "assets/models/*.png")

	file(GLOB_RECURSE _tga_assets RELATIVE "${CMAKE_CURRENT_LIST_DIR}/assets" CONFIGURE_DEPENDS "assets/models/*.tga")
	file(GLOB_RECURSE _tga_assets_full "assets/models/*.tga")

	file(GLOB_RECURSE _shaders_assets_full CONFIGURE_DEPENDS "src/shaders/*")

	add_custom_command(
		COMMAND
			${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/src/shaders ${CMAKE_CURRENT_LIST_DIR}/assets/shaders
		COMMAND
			${CMAKE_COMMAND} -E tar c ${PROJECT_OUTPUT_DIR}/${_asset_filename} -- cache fonts logos scenes textures shaders ${_gltf_assets} ${_bin_assets} ${_png_assets} ${_tga_assets}
		COMMAND
			${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_LIST_DIR}/assets/shaders
		OUTPUT
			${PROJECT_OUTPUT_DIR}/${_asset_filename}
		DEPENDS
			${_gltf_assets_full}
			${_bin_assets_full}
			${_png_assets_full}
			${_tga_assets_full}
			${_shaders_assets_full}
		WORKING_DIRECTORY 
			${CMAKE_CURRENT_LIST_DIR}/assets
	)

	add_custom_target(assets_tar DEPENDS ${PROJECT_OUTPUT_DIR}/${_asset_filename})
	add_dependencies(${PROJECT_EXE} assets_tar)

endif()

if (MSVC)
	target_compile_definitions(
		${PROJECT_LIB}
		PUBLIC
			_CRT_SECURE_NO_WARNINGS 
			UNICODE 
			_UNICODE
			NOMINMAX
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
endif()

if(XRSYSTEM_OPENVR)
	target_compile_definitions(
		${PROJECT_LIB}
		PRIVATE
			XRSYSTEM_OPENVR
	)

	target_link_libraries(
		${PROJECT_LIB}
		PRIVATE
			openvr
	)
endif()

## END COMPILE DEFINITIONS ##

if (SP_PACKAGE_RELEASE)
	if (MSVC)
		set_target_properties(${PROJECT_EXE} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
		set_target_properties(${PROJECT_LIB} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
	else()
		set_target_properties(${PROJECT_EXE} PROPERTIES INSTALL_RPATH "\$ORIGIN")
		set_target_properties(${PROJECT_EXE} PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
	endif()
endif()

if(SP_AUDIO)
	target_link_libraries(
		${PROJECT_LIB}
		PRIVATE
			fmod
			fmodstudio
	)

	target_compile_definitions(
		${PROJECT_LIB}
		PRIVATE
			SP_AUDIO
	)
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

# Change glomerate to use robin hood hashing
if(MSVC)
	list(APPEND SP_TARGET_OPTIONS /FI"robin_hood.h" -DGLOMERATE_MAP_TYPE=robin_hood::unordered_flat_map)
else()
    # GCC or Clang
	list(APPEND SP_TARGET_OPTIONS -include robin_hood.h -DGLOMERATE_MAP_TYPE=robin_hood::unordered_flat_map)
endif()

target_compile_options(${PROJECT_LIB} PRIVATE ${SP_TARGET_OPTIONS})
target_compile_options(${PROJECT_EXE} PRIVATE ${SP_TARGET_OPTIONS})

target_link_libraries(
	${PROJECT_LIB}
	PUBLIC
		${_physx_libraries} 
		GLEW
		glfw
		vhacd
		ImGui
		microtar
		murmurhash
		robin_hood
		tinygltfloader
		glomerate
		glm
		cxxopts
		OpenGL::GL
)

target_link_libraries(
	${PROJECT_EXE}
	PRIVATE
		${PROJECT_LIB}
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
		${PROJECT_LIB}
		glfw
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
