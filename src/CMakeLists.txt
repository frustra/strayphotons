#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(PROJECT_MAIN_WINIT_LIB ${PROJECT_BASE_NAME}-main-winit CACHE INTERNAL "Library implementing window creation using Winit")

set(PROJECT_XR_CORE_LIB ${PROJECT_BASE_NAME}-xr-core CACHE INTERNAL "Library containing generic XR/VR support")
set(PROJECT_XR_OPENVR_LIB ${PROJECT_BASE_NAME}-xr-openvr CACHE INTERNAL "Library implementing XR support using OpenVR")

# Vulkan desktop build
# TODO: Commented until package release saves a log file
# if(SP_PACKAGE_RELEASE AND WIN32)
#     add_executable(sp-vk WIN32)
# else()
add_executable(sp-vk
    main/glfw/MainGlfw.cc
    main/glfw/GlfwInputHandler.cc
    # xr/xr/XrManager.cc
    # xr/xr/openvr/vulkan/VkSubmit.cc
)
# endif()
target_link_libraries(sp-vk PRIVATE
    ${PROJECT_COMMON_EXE}
    ${PROJECT_LIB_NAME}
    ${PROJECT_COMMON_LIB}
    # ${PROJECT_XR_OPENVR_LIB}
    glfw
    Vulkan::Vulkan
)
target_compile_definitions(sp-vk PRIVATE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC)
set_target_properties(sp-vk PROPERTIES EXPORT_COMPILE_COMMANDS 1)

if(SP_BUILD_RUST)
    # Winit desktop build (dynamic linking)
    add_executable(sp-winit
        main/winit/MainWinit.cc
        # xr/xr/XrManager.cc
        # xr/xr/openvr/vulkan/VkSubmit.cc
    )
    target_link_libraries(sp-winit PRIVATE
        ${PROJECT_COMMON_EXE}
        ${PROJECT_LIB_NAME}
        ${PROJECT_MAIN_WINIT_LIB}
        # ${PROJECT_XR_OPENVR_LIB}
        Vulkan::Vulkan
    )
    target_compile_definitions(sp-winit PRIVATE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC)
endif()

# Headless executable used for CI testing purposes
# TODO: Commented until package release saves a log file
# if(SP_PACKAGE_RELEASE AND WIN32)
#     add_executable(sp-test WIN32)
# else()
add_executable(sp-test
    main/glfw/MainGlfw.cc
    main/glfw/GlfwInputHandler.cc
)
# endif()
target_link_libraries(sp-test PRIVATE
    ${PROJECT_COMMON_EXE}
    ${PROJECT_LIB_NAME}
    ${PROJECT_COMMON_LIB}
    glfw-egl
    Vulkan::Vulkan
)
target_compile_definitions(sp-test PRIVATE
    SP_GRAPHICS_SUPPORT_HEADLESS
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
)
set_target_properties(sp-test PROPERTIES EXPORT_COMPILE_COMMANDS 1)

add_subdirectory(main)
# add_subdirectory(xr)
