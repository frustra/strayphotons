#
# Stray Photons - Copyright (C) 2025 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(dynalo INTERFACE)

target_include_directories(dynalo INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/dynalo/include)

if(UNIX)
    target_link_libraries(dynalo INTERFACE dl)
elseif(WIN32)
    target_link_libraries(dynalo INTERFACE kernel32)
endif()
