
set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(glm ${CMAKE_CURRENT_BINARY_DIR}/glm)

# since GLM is a header-only library, these can safely be defined here
target_compile_definitions(
    glm
    INTERFACE
        GLM_FORCE_CTOR_INIT
        GLM_FORCE_CXX17
        GLM_FORCE_DEPTH_ZERO_TO_ONE
)
