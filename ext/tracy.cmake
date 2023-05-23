if(SP_PACKAGE_RELEASE)
    set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)
endif()
set(TRACY_ENABLE ${SP_ENABLE_TRACY} CACHE BOOL "" FORCE)
set(TRACY_ONLY_LOCALHOST ON CACHE BOOL "" FORCE)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-unused-but-set-variable -Wno-array-bounds)
endif()

add_subdirectory(tracy)
