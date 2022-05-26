if(SP_PACKAGE_RELEASE)
    set(TRACY_ENABLE OFF CACHE BOOL "" FORCE)
else()
    set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-unused-but-set-variable -Wno-array-bounds)
endif()

add_subdirectory(tracy)
