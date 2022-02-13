set(BUILD_EXAMPLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_DYNAMIC_LIBS OFF CACHE BOOL "" FORCE)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/wd4061 /wd4100 /wd4211 /wd4242 /wd4244 /wd4267 /wd4365 /wd4456 /wd4577 /wd4623 /wd4625)
    add_compile_options(/wd4626 /wd4668 /wd4706 /wd4800 /wd4820 /wd4996 /wd5026 /wd5027 /wd5045 /wd5219 /wd5246)
endif()

add_subdirectory(libsoundio) # target: libsoundio_static
add_compile_definitions(SOUNDIO_STATIC_LIBRARY)

target_include_directories(
    libsoundio_static
    INTERFACE
        ./libsoundio/
)
