#
# Stray Photons - Copyright (C) 2025 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

# Downgrade external packages to C++17
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(magic_enum)
add_subdirectory(robin-hood-hashing)
add_subdirectory(physx)

# These projects require more configuration that is encapsulated their own CMake files
include(cglm.cmake)
include(cxxopts.cmake)
include(dynalo.cmake)
include(fpng.cmake)
include(GameNetworkingSockets.cmake)
include(glfw.cmake)
include(glfw-egl.cmake)
include(glm.cmake)
include(imgui.cmake)
include(libnyquist.cmake)
include(linenoise.cmake)
include(microtar.cmake)
include(murmurhash.cmake)
include(openvr.cmake)
include(picojson.cmake)
include(resonance-audio.cmake)
include(SPIRV-Reflect.cmake)
include(Tecs.cmake)
include(tracy.cmake)
include(tinygltf.cmake)
include(vhacd.cmake)
include(VulkanMemoryAllocator.cmake)

if(WIN32)
    include(libsoundio.cmake)
endif()
