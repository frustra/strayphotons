
add_library(microtar STATIC microtar/src/microtar.c)

target_include_directories(
    microtar
    PUBLIC
        microtar/src
)

target_compile_definitions(
    microtar
    PUBLIC
        _CRT_SECURE_NO_WARNINGS
)