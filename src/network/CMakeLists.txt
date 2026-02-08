#
# Stray Photons - Copyright (C) 2026 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(${PROJECT_NETWORK_LIB} STATIC)

target_include_directories(${PROJECT_NETWORK_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NETWORK_LIB}
    PUBLIC
        ${PROJECT_CORE_LIB}
    PRIVATE
        ${PROJECT_COMMON_LIB}
        GameNetworkingSockets_s
)

target_compile_definitions(${PROJECT_NETWORK_LIB}
    PUBLIC
        SP_NETWORK_SUPPORT
)

# target_precompile_headers(${PROJECT_NETWORK_LIB} REUSE_FROM ${PROJECT_CORE_LIB})

add_subdirectory(network)
