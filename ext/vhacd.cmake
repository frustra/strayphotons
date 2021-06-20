option(NO_OPENCL "NO_OPENCL" ON)
option(NO_OPENMP "NO_OPENMP" ON)
set(CMAKE_COMMON_INC "../cmake_common.cmake")
set(BUILD_SHARED_LIBS off)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "8.0.0")
        add_compile_options(-Wno-error=stringop-overflow= -Wno-error=sign-compare -Wno-error=class-memaccess -Wno-error=unused-result)
    endif()
endif()

add_subdirectory(v-hacd/VHACD_Lib)
