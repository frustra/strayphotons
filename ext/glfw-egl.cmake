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

set(GLFW_USE_EGLHEADLESS ON CACHE BOOL "" FORCE)

add_subdirectory(glfw ${CMAKE_CURRENT_BINARY_DIR}/glfw-egl)
