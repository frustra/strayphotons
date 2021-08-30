# Downgrade external packages to C++17
set(CMAKE_CXX_STANDARD 17)

# No need to build the CXXOPTS tests
set(CXXOPTS_BUILD_TESTS OFF CACHE BOOL "Disable CXXOPTS Test Suite" FORCE)
set(CXXOPTS_BUILD_EXAMPLES OFF CACHE BOOL "Disable CXXOPTS Examples" FORCE)
add_subdirectory(cxxopts)

add_subdirectory(Tecs)
add_subdirectory(robin-hood-hashing)

# These projects require more configuration that is encapsulated their own CMake files
include(glew.cmake)
include(glfw.cmake)
include(glfw-egl.cmake)
include(glm.cmake)
include(imgui.cmake)
include(linenoise.cmake)
include(microtar.cmake)
include(murmurhash.cmake)
include(openvr.cmake)
include(PhysX.cmake)
include(picojson.cmake)
include(tinygltf.cmake)
include(vhacd.cmake)
include(VulkanMemoryAllocator.cmake)
