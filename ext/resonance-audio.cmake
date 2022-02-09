set(BUILD_RESONANCE_AUDIO_API ON CACHE INTERNAL "")

set(EIGEN3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/eigen/" CACHE PATH "")
set(PFFFT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/pffft/" CACHE PATH "")

add_subdirectory(resonance-audio)

# if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
#     target_compile_options(ResonanceAudioShared PRIVATE -include cstddef)
#     target_compile_options(ResonanceAudioStatic PRIVATE -include cstddef)
# endif()

target_include_directories(
    ResonanceAudioStatic
    INTERFACE
        ./resonance-audio/resonance_audio/api
)
