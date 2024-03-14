#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(${PROJECT_MAIN_WINIT_LIB} STATIC)

target_include_directories(${PROJECT_MAIN_WINIT_LIB}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${PROJECT_ROOT_DIR}/shared/include
)

target_link_libraries(${PROJECT_MAIN_WINIT_LIB} PUBLIC
    ${PROJECT_LIB_NAME}
    ${PROJECT_GRAPHICS_CORE_LIB}
    sp-rs-winit
    cxxopts
)

if(WIN32)
    target_link_libraries(${PROJECT_MAIN_WINIT_LIB} PUBLIC Imm32)
endif()

# target_precompile_headers(${PROJECT_MAIN_WINIT_LIB} REUSE_FROM ${PROJECT_CORE_LIB})

add_subdirectory(winit)
