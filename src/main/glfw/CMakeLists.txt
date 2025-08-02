#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

# Vulkan desktop build
if(SP_PACKAGE_RELEASE AND WIN32)
    add_executable(sp-vk WIN32
        MainGlfw.cc
        GlfwInputHandler.cc
    )
else()
    add_executable(sp-vk
        MainGlfw.cc
        GlfwInputHandler.cc
    )
endif()
target_link_libraries(sp-vk PRIVATE
    ${PROJECT_COMMON_EXE}
    ${PROJECT_LIB_NAME}
    ${PROJECT_COMMON_LIB}
    glfw
    Vulkan::Vulkan
)
set_target_properties(sp-vk PROPERTIES EXPORT_COMPILE_COMMANDS 1)

if(WIN32)
    add_custom_command(TARGET sp-vk PRE_LINK
        COMMAND
            ./extra/remove_in_use.py sp-vk exe pdb
        WORKING_DIRECTORY ${PROJECT_ROOT_DIR}
    )
endif()

# Headless executable used for CI testing purposes
if(SP_PACKAGE_RELEASE AND WIN32)
    add_executable(sp-test WIN32
        MainGlfw.cc
        GlfwInputHandler.cc
    )
else()
    add_executable(sp-test
        MainGlfw.cc
        GlfwInputHandler.cc
    )
endif()
target_link_libraries(sp-test PRIVATE
    ${PROJECT_COMMON_EXE}
    ${PROJECT_LIB_NAME}
    ${PROJECT_COMMON_LIB}
    glfw-egl
    Vulkan::Vulkan
)
target_compile_definitions(sp-test PRIVATE SP_TEST_MODE SP_GRAPHICS_SUPPORT_HEADLESS)
# set_target_properties(sp-test PROPERTIES EXPORT_COMPILE_COMMANDS 1)
