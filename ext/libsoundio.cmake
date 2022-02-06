set(BUILD_EXAMPLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_DYNAMIC_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(libsoundio) # target: libsoundio_static
add_compile_definitions(SOUNDIO_STATIC_LIBRARY)

target_include_directories(
    libsoundio_static
    INTERFACE
        ./libsoundio/
)
