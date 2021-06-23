set(PROJECT_GRAPHICS_CORE_LIB ${PROJECT_BASE_NAME}-graphics-core CACHE INTERNAL "Library containing context-independent graphics code")
set(PROJECT_GRAPHICS_HEADLESS_LIB ${PROJECT_BASE_NAME}-graphics-headless CACHE INTERNAL "Library containing a minimal headless graphics implementation")
set(PROJECT_GRAPHICS_EGL_LIB ${PROJECT_BASE_NAME}-graphics-egl CACHE INTERNAL "Library containing an OpenGL-based graphics implementation using EGL")
set(PROJECT_GRAPHICS_OPENGL_LIB ${PROJECT_BASE_NAME}-graphics-opengl CACHE INTERNAL "Library containing an OpenGL-based graphics implementation using OpenGL 4.0")

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
    # ${PROJECT_LIB} # TODO: Remove this dependency
    glfw-egl
)
target_link_libraries(${PROJECT_GRAPHICS_OPENGL_LIB} PRIVATE
    ${PROJECT_CORE_LIB}
    ${PROJECT_GRAPHICS_CORE_LIB}
    # ${PROJECT_LIB} # TODO: Remove this dependency
    glfw
)

add_subdirectory(graphics)

# TODO: Clean this up
target_sources(${PROJECT_LIB} PRIVATE
    GraphicsManager.cc
    GuiRenderer.cc
)
