project(sp)
set(CMAKE_MIN_VERSION 3.12)
cmake_minimum_required(VERSION ${CMAKE_MIN_VERSION} FATAL_ERROR)


set(SP_VERSION_MAJOR "0")
set(SP_VERSION_MINOR "1")
set(SP_VERSION "${SP_VERSION_MAJOR}.${SP_VERSION_MINOR}")
set(SP_PACKAGE_RELEASE FALSE CACHE BOOL "Build packaged release")
set(SP_ENABLE_VR FALSE CACHE BOOL "Build with VR support")

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
	list(APPEND SP_TARGET_OPTIONS -DPACKAGE_RELEASE)
endif()

if (SP_ENABLE_VR)
	message(STATUS "Enabling VR support")
	list(APPEND SP_TARGET_OPTIONS -DENABLE_VR)
endif()

list(APPEND SP_TARGET_OPTIONS -DGLM_FORCE_CTOR_INIT)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (UNIX)
	list(APPEND SP_TARGET_OPTIONS -Wall)
	list(APPEND SP_TARGET_OPTIONS -Wno-error=strict-aliasing -Wno-error=parentheses)
endif()

if (MSVC)
	list(APPEND SP_TARGET_OPTIONS -D_CRT_SECURE_NO_WARNINGS -DUNICODE -D_UNICODE /wd4800 /MP)

	if (SP_DEBUG)
		list(APPEND SP_TARGET_OPTIONS /MDd)
	else()
		list(APPEND SP_TARGET_OPTIONS /MD)
	endif()
endif()

if (SP_DEBUG)
	list(APPEND SP_TARGET_OPTIONS -D_DEBUG)
else()
	list(APPEND SP_TARGET_OPTIONS -DNDEBUG)
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

set(EXT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/ext)

find_package(OpenGL REQUIRED)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
set(GLFW_INSTALL OFF CACHE BOOL "")
if (WIN32)
	set(GLFW_USE_HYBRID_HPG ON)
endif()
add_subdirectory(${EXT_PATH}/glfw)

set(GOOGLETEST_DIR ${EXT_PATH}/googletest/googletest)
add_subdirectory(${EXT_PATH}/googletest)

add_subdirectory(${EXT_PATH}/v-hacd)

function(ADD_PHYSX_SUBDIRECTORY)
	set(PX_OUTPUT_ARCH x86)
	set(PHYSX_ROOT_DIR ${EXT_PATH}/physx/physx)
	set(PX_OUTPUT_LIB_DIR ${PROJECT_OUTPUT_DIR}/physx)
	set(PX_OUTPUT_BIN_DIR ${PX_OUTPUT_LIB_DIR})
	set(PX_GENERATE_STATIC_LIBRARIES OFF CACHE BOOL "")

	set(ENV{PM_PATHS} ${EXT_PATH}/physx/externals/opengl-linux)
	set(ENV{PM_CMakeModules_PATH} ${EXT_PATH}/physx/externals/cmakemodules)
	set(ENV{PM_PxShared_PATH} ${EXT_PATH}/physx/pxshared)

	if (APPLE)
		set(TARGET_BUILD_PLATFORM mac)
		set(OUR_PHYSX_CXX_FLAGS_FOR_APPLE "-faligned-allocation -Wno-atomic-implicit-seq-cst")

		set(PHYSX_CXX_FLAGS_DEBUG "-O0 -g ${OUR_PHYSX_CXX_FLAGS_FOR_APPLE}" CACHE INTERNAL "")
		set(PHYSX_CXX_FLAGS_CHECKED "-O3 -g ${OUR_PHYSX_CXX_FLAGS_FOR_APPLE}" CACHE INTERNAL "")
		set(PHYSX_CXX_FLAGS_PROFILE "-O3 -g ${OUR_PHYSX_CXX_FLAGS_FOR_APPLE}" CACHE INTERNAL "")
		set(PHYSX_CXX_FLAGS_RELEASE "-O3 -g ${OUR_PHYSX_CXX_FLAGS_FOR_APPLE}" CACHE INTERNAL "")
		set(NV_FORCE_64BIT_SUFFIX TRUE)
		set(NV_FORCE_32BIT_SUFFIX FALSE)
	elseif (UNIX)
		set(TARGET_BUILD_PLATFORM linux)

		set(CMAKE_SYSTEM_NAME Linux)
		set(CMAKE_SYSTEM_VERSION 1)
		set(CMAKE_SYSROOT ${LINUX_ROOT})
		set(CMAKE_LIBRARY_ARCHITECTURE x86_64-linux-gnu)
	elseif (MSVC)
		set(TARGET_BUILD_PLATFORM windows)
		set(NV_USE_STATIC_WINCRT FALSE CACHE BOOL "")

		if (SP_DEBUG)
			set(NV_USE_DEBUG_WINCRT TRUE CACHE BOOL "")
		else()
			set(NV_USE_DEBUG_WINCRT FALSE CACHE BOOL "")
		endif()
	endif()
	add_subdirectory(${EXT_PATH}/physx/physx/compiler/public)
endfunction()

ADD_PHYSX_SUBDIRECTORY()

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${EXT_PATH}/physx/physx/include
	${EXT_PATH}/physx/PxShared/include
	${CMAKE_CURRENT_SOURCE_DIR}/src
	${EXT_PATH}/glfw/include
	${EXT_PATH}/glew/include
	${EXT_PATH}/glm
	${EXT_PATH}/tinygltfloader
	${EXT_PATH}/glomerate/include
	${EXT_PATH}/fmod/include/fmod
	${EXT_PATH}/fmod/include/fmodstudio
	${EXT_PATH}/cxxopts/include
	${EXT_PATH}/microtar/src
	${EXT_PATH}
)

if (SP_ENABLE_VR)
	include_directories(${EXT_PATH}/openvr/headers)
endif()

add_definitions(-DGLEW_STATIC=1)
add_library(GLEW STATIC ${EXT_PATH}/glew/src/glew.c)

file(GLOB ImGui_sources ${EXT_PATH}/imgui/imgui.cpp ${EXT_PATH}/imgui/imgui_draw.cpp ${EXT_PATH}/imgui/imgui_demo.cpp)
add_library(ImGui STATIC ${ImGui_sources})

if (NOT WIN32)
	add_library(linenoise STATIC ${EXT_PATH}/linenoise/linenoise.c)
endif()

add_library(microtar STATIC ${EXT_PATH}/microtar/src/microtar.c)
add_library(murmurhash STATIC ${EXT_PATH}/murmurhash/MurmurHash3.cpp)


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
	${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.glsl
	${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.vert
	${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.tesc
	${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.tese
	${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.geom
	${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.frag
	${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.comp)
list(REMOVE_DUPLICATES all_shaders)

add_library(${PROJECT_LIB} ${all_sources} ${all_headers} ${all_shaders})

# Configure glomerate to use 32 bit entities on WIN32
if (WIN32)
	target_compile_definitions(${PROJECT_LIB} PRIVATE "-DGLOMERATE_32BIT_ENTITIES")
endif()

set (PHYSX_LIBRARIES
	"PhysX"
	"PhysXCharacterKinematic"
	"PhysXCommon"
	"PhysXCooking"
	"PhysXExtensions"
	"PhysXFoundation"
	"PhysXPvdSDK"
)

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
elseif (APPLE)
	set (FMOD_LOADS "fmod" "fmodstudio")
	set (FMOD_ARCH "apple")
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

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	target_link_libraries(${PROJECT_LIB} stdc++fs)
endif()

if (UNIX)
	list(APPEND SP_TARGET_OPTIONS -Werror -Wsign-compare)
endif()

if (APPLE)
	list(APPEND SP_TARGET_OPTIONS -Wno-unused-local-typedef)
endif()

target_compile_options(${PROJECT_LIB} PRIVATE ${SP_TARGET_OPTIONS})
target_compile_options(${PROJECT_EXE} PRIVATE ${SP_TARGET_OPTIONS})

set_target_properties(${PROJECT_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})
set_target_properties(${PROJECT_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_OUTPUT_DIR}/Release)
set_target_properties(${PROJECT_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_OUTPUT_DIR}/Debug)

if (SP_PACKAGE_RELEASE)
	if (MSVC)
		set_target_properties(${PROJECT_EXE} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
	else()
		set_target_properties(${PROJECT_EXE} PROPERTIES INSTALL_RPATH "\$ORIGIN")
		set_target_properties(${PROJECT_EXE} PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
	endif()
endif()

target_link_libraries(${PROJECT_LIB}
	glfw
	${GLFW_LIBRARIES}
)

target_link_libraries(${PROJECT_EXE}
	${PROJECT_LIB}
	${OPENGL_LIBRARIES}
	GLEW
	glfw
	vhacd
	${GLFW_LIBRARIES}
	ImGui
	microtar
	murmurhash
)

if (SP_ENABLE_VR)
	target_link_libraries(${PROJECT_EXE} ${EXT_PATH}/openvr/lib/win32/openvr_api.lib)
endif()

if (NOT WIN32)
	target_link_libraries(${PROJECT_EXE} linenoise)
endif()


if (CMAKE_VERSION VERSION_GREATER 3.6)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT sp)
endif()

################################################
# Build tests
################################################
add_subdirectory(tests)
