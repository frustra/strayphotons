
# These projects already have functional CMake scripts that can be used as part of the native build
add_subdirectory(googletest)

# No need to build the GLM unit tests
set(GLM_TEST_ENABLE OFF CACHE BOOL "Disable GLM Test Suite" FORCE)
add_subdirectory(glm)
# since GLM is a header-only library, these can safely be defined here
target_compile_definitions(
    glm
    INTERFACE
        GLM_FORCE_CTOR_INIT
        GLM_FORCE_CXX17
)

# No need to build the CXXOPTS tests
set(CXXOPTS_BUILD_TESTS OFF CACHE BOOL "Disable CXXOPTS Test Suite" FORCE)
set(CXXOPTS_BUILD_EXAMPLES OFF CACHE BOOL "Disable CXXOPTS Examples" FORCE)
add_subdirectory(cxxopts)

add_subdirectory(Tecs)
add_subdirectory(robin-hood-hashing)

# These projects require more configuration that is encapsulated their own CMake files
include(PhysX.cmake)
include(glfw.cmake)
include(glfw-egl.cmake)
include(imgui.cmake)
include(glew.cmake)
include(linenoise.cmake)
include(openvr.cmake)
include(tinygltf.cmake)
include(microtar.cmake)
include(glomerate.cmake)
include(murmurhash.cmake)
include(vhacd.cmake)
include(picojson.cmake)
include(protobuf.cmake)
