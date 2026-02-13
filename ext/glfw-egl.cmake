#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(GLFW_TARGET_NAME glfw-egl CACHE STRING "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

if (WIN32)
    set(GLFW_USE_HYBRID_HPG ON)
endif()

# Force GLFW in headless EGL mode
set(GLFW_BUILD_WIN32 OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_COCOA OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)

add_subdirectory(glfw ${CMAKE_CURRENT_BINARY_DIR}/glfw-egl)
