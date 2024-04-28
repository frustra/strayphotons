#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(${PROJECT_AUDIO_LIB} STATIC)

target_include_directories(${PROJECT_AUDIO_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_AUDIO_LIB}
    PUBLIC
        ${PROJECT_CORE_LIB}
        libnyquist
    PRIVATE
        ${PROJECT_COMMON_LIB}
        libsoundio_static
        ${LIBSOUNDIO_LIBS}
        ResonanceAudioStatic
)

target_compile_definitions(${PROJECT_AUDIO_LIB}
    PUBLIC
        SP_AUDIO_SUPPORT
    PRIVATE
        SOUNDIO_STATIC_LIBRARY
)

target_precompile_headers(${PROJECT_AUDIO_LIB} REUSE_FROM ${PROJECT_CORE_LIB})

add_subdirectory(audio)
