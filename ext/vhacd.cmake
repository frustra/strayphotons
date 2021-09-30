option(NO_OPENCL "NO_OPENCL" ON)
option(NO_OPENMP "NO_OPENMP" ON)
set(CMAKE_COMMON_INC "../cmake_common.cmake")
set(BUILD_SHARED_LIBS off)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "8.0.0")
        add_compile_options(-Wno-stringop-overflow -Wno-class-memaccess -Wno-unused-result -Wno-maybe-uninitialized -Wno-unknown-pragmas -Wno-misleading-indentation -Wno-strict-aliasing)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wno-unknown-pragmas -Wno-misleading-indentation)
endif()

add_subdirectory(v-hacd/VHACD_Lib)
