#
# Stray Photons - Copyright (C) 2026 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "Disable TinyGLTF building examples" FORCE)
set(TINYGLTF_HEADER_ONLY ON CACHE BOOL "Turn on header-only mode" FORCE)
set(TINYGLTF_INSTALL OFF CACHE BOOL "Don't install since we use add_subdirectory" FORCE)
set(TINYGLTF_INSTALL_VENDOR OFF CACHE BOOL "Don't install since we use add_subdirectory" FORCE)
add_subdirectory(tinygltf)

target_compile_definitions(tinygltf INTERFACE TINYGLTF_NO_FS=1)
