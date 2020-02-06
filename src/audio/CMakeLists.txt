
if(NOT SP_AUDIO)
    return()
endif()

target_sources(
    ${PROJECT_LIB}
    PRIVATE
        AudioManager.cc
)