add_library(
    SPIRV-Reflect
    STATIC 
        SPIRV-Reflect/spirv_reflect.c
        SPIRV-Reflect/common/output_stream.cpp
)

target_include_directories(SPIRV-Reflect PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Reflect
)
