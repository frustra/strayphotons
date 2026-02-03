#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(fpng STATIC fpng/src/fpng.cpp)

target_include_directories(fpng PUBLIC ./fpng/src)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(fpng PRIVATE -msse4.1 -mpclmul)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
    target_compile_options(fpng PRIVATE -msse4.1 -mpclmul -Wno-tautological-constant-out-of-range-compare -Wno-unused-function)
endif()
