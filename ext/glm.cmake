#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(glm ${CMAKE_CURRENT_BINARY_DIR}/glm)

# since GLM is a header-only library, these can safely be defined here
target_compile_definitions(
    glm
    INTERFACE
        GLM_FORCE_CTOR_INIT
        GLM_FORCE_CXX2A
        GLM_FORCE_DEPTH_ZERO_TO_ONE
)
