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
    #         WinitInputHandler.cc
    #     )
    # else()
    add_executable(sp-winit
        MainWinit.cc
        WinitInputHandler.cc
    )
    # endif()
    target_link_libraries(sp-winit PRIVATE
        ${PROJECT_COMMON_EXE}
        ${PROJECT_LIB_NAME}
        ${PROJECT_XR_OPENVR_LIB}
        sp-rs-winit
        # cxxopts
        Vulkan::Vulkan
    )
    target_compile_definitions(sp-winit PRIVATE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC)

    # target_include_directories(sp-winit
    #     PUBLIC
    #         ${CMAKE_CURRENT_SOURCE_DIR}
    #     PRIVATE
    #         ${PROJECT_ROOT_DIR}/include
    # )

    if(WIN32)
        target_link_libraries(sp-winit PUBLIC Imm32)
    endif()
endif()
