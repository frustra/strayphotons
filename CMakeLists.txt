project(sp)
set(CMAKE_MIN_VERSION 3.1)
cmake_minimum_required(VERSION ${CMAKE_MIN_VERSION} FATAL_ERROR)


set(SP_VERSION_MAJOR "0")
set(SP_VERSION_MINOR "1")
set(SP_VERSION "${SP_VERSION_MAJOR}.${SP_VERSION_MINOR}")
set(SP_PACKAGE_RELEASE FALSE CACHE BOOL "Build packaged release")


if (CMAKE_BUILD_TYPE MATCHES Release)
	message(STATUS "sp target is Release")
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
	add_definitions(-DPACKAGE_RELEASE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

if (UNIX)
	add_definitions(-Wall)
	add_definitions(-Wno-error=strict-aliasing)

	if (SP_DEBUG)
		add_definitions(-D_DEBUG)
	endif()
endif()

if (MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -DUNICODE -D_UNICODE /wd4800 /MP)

	if (SP_DEBUG)
		add_definitions(/MDd)
	else()
		add_definitions(/MD)
	endif()
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
add_subdirectory(${EXT_PATH}/glfw)

set(GOOGLETEST_DIR ${EXT_PATH}/googletest/googletest)
add_subdirectory(${EXT_PATH}/googletest)

add_subdirectory(${EXT_PATH}/v-hacd)


include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${EXT_PATH}/physx/PhysXSDK/Include
	${CMAKE_CURRENT_SOURCE_DIR}/src
	${EXT_PATH}/glfw/include
	${EXT_PATH}/glew/include
	${EXT_PATH}/boostconfig/include
	${EXT_PATH}/glm
	${EXT_PATH}/tinygltfloader
	${EXT_PATH}/glomerate/include
	${EXT_PATH}/boost
	${EXT_PATH}/fmod/include/fmod
	${EXT_PATH}/fmod/include/fmodstudio
	${EXT_PATH}/cxxopts/include
	${EXT_PATH}/microtar/src
	${EXT_PATH}
)

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
	target_compile_definitions(${PROJECT_LIB}
		PRIVATE "-DGLOMERATE_32BIT_ENTITIES")
endif()

# load PhysX
if (SP_DEBUG)
	set (PHYSX_ENV DEBUG)
else()
	set (PHYSX_ENV "")
endif()

if (WIN32)
	set (PHYSX_ARCH _x86)
elseif (APPLE)
	set (PHYSX_ARCH "")
else()
	set (PHYSX_ARCH _x64)
endif()

set (PHYSX_LOADS
	"PhysX3CharacterKinematic${PHYSX_ENV}${PHYSX_ARCH}"
	"PhysX3Common${PHYSX_ENV}${PHYSX_ARCH}"
	"PhysX3Cooking${PHYSX_ENV}${PHYSX_ARCH}"
	"PhysX3Extensions${PHYSX_ENV}"
	"PhysX3${PHYSX_ENV}${PHYSX_ARCH}"
)

if (NOT SP_PACKAGE_RELEASE)
	set(PHYSX_LOADS ${PHYSX_LOADS}
		"PhysXVisualDebuggerSDK${PHYSX_ENV}"
	)
endif()

if (APPLE)
	# for some reason on OS X linking fails without all the libs
	set (PHYSX_LOADS ${PHYSX_LOADS}
		"LowLevel${PHYSX_ENV}"
		"LowLevelCloth${PHYSX_ENV}"
		"PvdRuntime${PHYSX_ENV}"
		"PxTask${PHYSX_ENV}"
		"SimulationController${PHYSX_ENV}"
		"SceneQuery${PHYSX_ENV}"
		"PhysXProfileSDK${PHYSX_ENV}"
	)
endif()

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

if (SP_DEBUG)
	set(BOOST_VARIANT _DEBUG)
else()
	set(BOOST_VARIANT "")
endif()

set(BOOST_LIBS
	boost_filesystem${BOOST_VARIANT}
	boost_system${BOOST_VARIANT}
)

foreach (BOOST_LIB ${BOOST_LIBS})
	message(STATUS ${BOOST_LIB})
	find_library(FOUND_LIB_${BOOST_LIB}
		NAMES ${BOOST_LIB} lib${BOOST_LIB}
		PATHS ${CMAKE_CURRENT_SOURCE_DIR}/build
		NO_DEFAULT_PATH
	)
	LIST(APPEND BOOST_LIBRARIES ${FOUND_LIB_${BOOST_LIB}})
endforeach (BOOST_LIB)

add_executable(${PROJECT_EXE} ${PROJECT_MAIN_CC})
target_link_libraries(${PROJECT_EXE} ${PROJECT_LIB})
target_link_libraries(${PROJECT_LIB} ${PHYSX_LIBRARIES} ${FMOD_LIBRARIES} ${BOOST_LIBRARIES})

# C++11
target_compile_features(${PROJECT_LIB} PRIVATE cxx_range_for)
target_compile_features(${PROJECT_EXE} PRIVATE cxx_range_for)

if (UNIX)
	set(PROJECT_COMPILE_OPTIONS
		-Werror
		-Wsign-compare
	)

	target_compile_options(${PROJECT_LIB} PRIVATE ${PROJECT_COMPILE_OPTIONS})
	target_compile_options(${PROJECT_EXE} PRIVATE ${PROJECT_COMPILE_OPTIONS})
endif()

if (APPLE)
	target_compile_options(${PROJECT_LIB} PRIVATE "-Wno-unused-local-typedef")
	target_compile_options(${PROJECT_EXE} PRIVATE "-Wno-unused-local-typedef")
endif()

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
