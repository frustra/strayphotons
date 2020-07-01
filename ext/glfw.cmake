
set(GLFW_TARGET_NAME glfw CACHE STRING "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

if (WIN32)
	set(GLFW_USE_HYBRID_HPG ON)
endif()

set(GLFW_USE_EGLHEADLESS OFF CACHE BOOL "" FORCE)

add_subdirectory(glfw ${CMAKE_CURRENT_BINARY_DIR}/glfw)
