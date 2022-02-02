option(TRACY_ENABLE "" ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-unused-but-set-variable -Wno-array-bounds)
endif()

add_subdirectory(tracy)
