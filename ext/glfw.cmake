#
# Stray Photons - Copyright (C) 2026 Jacob Wirth & Justine Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(GLFW_TARGET_NAME glfw CACHE STRING "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

if (WIN32)
    set(GLFW_USE_HYBRID_HPG ON)
else()
    set(GLFW_BUILD_WIN32 OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_COCOA ON CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
endif()

add_subdirectory(glfw ${CMAKE_CURRENT_BINARY_DIR}/glfw)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(glfw PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wno-pedantic>)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
    target_compile_options(glfw PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wno-pedantic>)
endif()
