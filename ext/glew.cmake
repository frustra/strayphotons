
add_library(
    GLEW 
    STATIC 
        glew/src/glew.c
)

target_include_directories(
    GLEW 
    PUBLIC 
        glew/include
)

target_compile_definitions(
    GLEW
    PUBLIC
        GLEW_STATIC=1
)