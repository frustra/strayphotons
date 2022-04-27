set(BUILD_RESONANCE_AUDIO_API OFF CACHE INTERNAL "")

set(EIGEN3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/eigen/" CACHE PATH "")
set(PFFFT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/pffft/" CACHE PATH "")

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    foreach(flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG)
        STRING(REGEX REPLACE "/RTC[^ ]*" "" ${flag_var} "${${flag_var}}")
    endforeach(flag_var)
endif()

add_subdirectory(resonance-audio)

add_library(ResonanceAudioStatic STATIC $<TARGET_OBJECTS:ResonanceAudioObj>
        $<TARGET_OBJECTS:SadieHrtfsObj>
        $<TARGET_OBJECTS:PffftObj>)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(ResonanceAudioObj PRIVATE "/wd4127")
    target_compile_options(ResonanceAudioObj PRIVATE "/O2")
    target_compile_options(SadieHrtfsObj PRIVATE "/O2")
    target_compile_options(PffftObj PRIVATE "/O2")
endif()

target_include_directories(
    ResonanceAudioStatic
    INTERFACE
        ./resonance-audio/resonance_audio/api
)
