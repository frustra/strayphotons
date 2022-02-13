set(BUILD_EXAMPLE OFF CACHE INTERNAL "")

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/wd4028 /wd4245 /wd4456 /wd4514 /wd4701 /wd4702 /wd4706)
endif()

add_subdirectory(libnyquist)
