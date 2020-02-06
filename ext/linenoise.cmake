
# linenoise is not available on Win32
if (WIN32)
    return()
endif()

add_library(
    linenoise 
    STATIC 
        linenoise/linenoise.c
)

target_include_directories(
    linenoise
    PUBLIC
        linenoise
)