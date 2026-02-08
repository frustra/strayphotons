#
# Stray Photons - Copyright (C) 2026 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(BUILD_STATIC_LIB ON CACHE INTERNAL "")
set(BUILD_SHARED_LIB OFF CACHE INTERNAL "")
set(LTO ON CACHE INTERNAL "")
set(Protobuf_USE_STATIC_LIBS ON CACHE INTERNAL "")

# if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
#     add_compile_options(/wd4028 /wd4245 /wd4456 /wd4514 /wd4701 /wd4702 /wd4706)
# elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
#     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-stringop-overread")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-reorder -Wno-unused-variable -Wno-sign-compare -Wno-range-loop-construct")
# elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
#     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-shift-negative-value -Wno-macro-redefined -Wno-static-in-inline")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-reorder-ctor -Wno-unused-private-field -Wno-range-loop-construct -Wno-tautological-overlap-compare")
# endif()

add_subdirectory(GameNetworkingSockets)
