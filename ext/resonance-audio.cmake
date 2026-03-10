#
# Stray Photons - Copyright (C) 2026 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(BUILD_RESONANCE_AUDIO_API OFF CACHE INTERNAL "")

set(EIGEN3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/eigen/" CACHE PATH "")
set(PFFFT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/pffft/" CACHE PATH "")

add_subdirectory(resonance-audio)

add_library(ResonanceAudioStatic STATIC
    $<TARGET_OBJECTS:ResonanceAudioObj>
    $<TARGET_OBJECTS:SadieHrtfsObj>
    $<TARGET_OBJECTS:PffftObj>
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(ResonanceAudioObj PRIVATE /wd4127 /wd4702)
endif()

target_include_directories(ResonanceAudioStatic INTERFACE
    ./resonance-audio/
)
