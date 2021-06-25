add_library(${PROJECT_GRAPHICS_CORE_LIB} STATIC)
add_library(${PROJECT_GRAPHICS_HEADLESS_LIB} INTERFACE)
add_library(${PROJECT_GRAPHICS_EGL_LIB} STATIC)
add_library(${PROJECT_GRAPHICS_OPENGL_LIB} STATIC)

target_include_directories(${PROJECT_GRAPHICS_CORE_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_GRAPHICS_CORE_LIB} PRIVATE
    ${PROJECT_CORE_LIB}
)

target_link_libraries(${PROJECT_GRAPHICS_HEADLESS_LIB} INTERFACE
	${PROJECT_GRAPHICS_CORE_LIB}
)
target_link_libraries(${PROJECT_GRAPHICS_EGL_LIB} PRIVATE
    ${PROJECT_CORE_LIB}
    ${PROJECT_GRAPHICS_CORE_LIB}
    ${PROJECT_INPUT_CORE_LIB}
    GLEW
    glfw-egl
    ImGui
    OpenGL::GL
)
target_link_libraries(${PROJECT_GRAPHICS_OPENGL_LIB} PRIVATE
    ${PROJECT_CORE_LIB}
    ${PROJECT_GRAPHICS_CORE_LIB}
    ${PROJECT_INPUT_CORE_LIB}
    GLEW
    glfw
    ImGui
    OpenGL::GL
)

target_compile_definitions(${PROJECT_GRAPHICS_CORE_LIB} PUBLIC
    SP_GRAPHICS_SUPPORT
)
target_compile_definitions(${PROJECT_GRAPHICS_HEADLESS_LIB} INTERFACE
    SP_GRAPHICS_SUPPORT_HEADLESS
)
target_compile_definitions(${PROJECT_GRAPHICS_EGL_LIB} PUBLIC
    SP_GRAPHICS_SUPPORT_GL
)
target_compile_definitions(${PROJECT_GRAPHICS_OPENGL_LIB} PUBLIC
    SP_GRAPHICS_SUPPORT_GL
)

add_subdirectory(graphics)

target_sources(${PROJECT_EXE} PRIVATE
    GraphicsManager.cc
)

target_sources(${PROJECT_HEADLESS_EXE} PRIVATE
    GraphicsManager.cc
)

target_sources(${PROJECT_TEST_EXE} PRIVATE
    GraphicsManager.cc
)
