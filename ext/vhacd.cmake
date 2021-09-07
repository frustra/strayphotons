option(NO_OPENCL "NO_OPENCL" ON)
option(NO_OPENMP "NO_OPENMP" ON)
set(CMAKE_COMMON_INC "../cmake_common.cmake")
set(BUILD_SHARED_LIBS off)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "8.0.0")
        add_compile_options(-Wno-stringop-overflow -Wno-sign-compare -Wno-class-memaccess -Wno-unused-result -Wno-maybe-uninitialized)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/wd4005 /wd4302 /wd4311 /wd4456 /wd4701 /wd4706)
endif()

add_subdirectory(v-hacd/VHACD_Lib)
