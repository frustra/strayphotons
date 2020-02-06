
add_library(glomerate INTERFACE)

target_include_directories(
    glomerate
    INTERFACE
        glomerate/include   
)

target_compile_definitions(
    glomerate
    INTERFACE 
        GLOMERATE_32BIT_ENTITIES
)