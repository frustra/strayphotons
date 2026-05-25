#
# Stray Photons - Copyright (C) 2026 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

project(sp-sdk VERSION 0.1)

if(NOT DEFINED PROJECT_LIB_NAME)
    set(PROJECT_LIB_NAME sp)
endif()

if(NOT DEFINED PROJECT_SDK_LIB)
    set(PROJECT_SDK_LIB strayphotons-sdk)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ext)
    add_subdirectory(ext/glm)
    add_subdirectory(ext/magic_enum)
    add_subdirectory(ext/robin-hood-hashing)
    add_subdirectory(ext/Tecs)
endif()

add_library(${PROJECT_SDK_LIB} INTERFACE)

target_include_directories(${PROJECT_SDK_LIB} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${PROJECT_SDK_LIB} INTERFACE ${PROJECT_LIB_NAME} Tecs)

add_subdirectory(cpp)
