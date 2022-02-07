set(BUILD_RESONANCE_AUDIO_API ON CACHE INTERNAL "")

set(EIGEN3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/eigen/" CACHE PATH "")
set(PFFFT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/pffft/" CACHE PATH "")

add_subdirectory(resonance-audio)

target_include_directories(
    ResonanceAudioStatic
    INTERFACE
        ./resonance-audio/resonance_audio/api
)
