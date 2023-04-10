add_library(${PROJECT_AUDIO_LIB} STATIC)

target_include_directories(${PROJECT_AUDIO_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_AUDIO_LIB} PUBLIC
    ${PROJECT_CORE_LIB}
    ${PROJECT_GAME_LIB}
    libsoundio_static
    ${LIBSOUNDIO_LIBS}
    ResonanceAudioStatic
    libnyquist
)

target_compile_definitions(${PROJECT_AUDIO_LIB} PUBLIC
    SP_AUDIO_SUPPORT
    SOUNDIO_STATIC_LIBRARY
)

# target_precompile_headers(${PROJECT_AUDIO_LIB} REUSE_FROM ${PROJECT_CORE_LIB})

add_subdirectory(audio)
