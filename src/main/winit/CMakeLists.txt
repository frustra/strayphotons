#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

if(SP_BUILD_RUST)
    # Winit desktop build (dynamic linking)
    # TODO: Commented until package release saves a log file
    # if(SP_PACKAGE_RELEASE AND WIN32)
    #     add_executable(sp-winit WIN32
    #         MainWinit.cc
    #     )
    # else()
    add_executable(sp-winit
        MainWinit.cc
    )
    # endif()
    target_link_libraries(sp-winit PRIVATE
        ${PROJECT_COMMON_EXE}
        ${PROJECT_LIB_NAME}
        sp-rs-winit
        Vulkan::Vulkan
    )

    if(WIN32)
        target_link_libraries(sp-winit PUBLIC Imm32)
    endif()
endif()
