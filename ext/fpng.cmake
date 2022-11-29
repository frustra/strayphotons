
add_library(fpng STATIC fpng/src/fpng.cpp)

target_include_directories(fpng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/fpng/src)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(fpng PRIVATE -msse4.1 -mpclmul)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
    target_compile_options(fpng PRIVATE -msse4.1 -mpclmul -Wno-tautological-constant-out-of-range-compare -Wno-unused-function)
endif()
