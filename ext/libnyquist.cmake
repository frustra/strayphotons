#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(BUILD_EXAMPLE OFF CACHE INTERNAL "")

add_subdirectory(libnyquist)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(libnyquist PRIVATE /wd4028 /wd4113 /wd4245 /wd4456 /wd4514 /wd4701 /wd4702 /wd4706)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(libnyquist PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wno-stringop-overread>)
    target_compile_options(libnyquist PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-reorder -Wno-unused-variable -Wno-sign-compare -Wno-range-loop-construct>)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
    target_compile_options(libnyquist PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wno-shift-negative-value -Wno-macro-redefined -Wno-static-in-inline>)
    target_compile_options(libnyquist PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-reorder-ctor -Wno-unused-private-field -Wno-range-loop-construct -Wno-tautological-overlap-compare -Wno-invalid-utf8>)
endif()
