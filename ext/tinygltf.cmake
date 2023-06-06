#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "Disable TinyGLTF building examples" FORCE)
add_subdirectory(tinygltf)

add_library(tinygltf INTERFACE)

target_compile_definitions(tinygltf INTERFACE TINYGLTF_NO_FS=1)

target_include_directories(tinygltf SYSTEM INTERFACE ./tinygltf)
