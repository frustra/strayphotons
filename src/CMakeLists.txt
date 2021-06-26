set(PROJECT_CORE_LIB ${PROJECT_BASE_NAME}-core CACHE INTERNAL "Library containing core components such as the ECS and asset loading")

set(PROJECT_GRAPHICS_CORE_LIB ${PROJECT_BASE_NAME}-graphics-core CACHE INTERNAL "Library containing context-independent graphics code")
set(PROJECT_GRAPHICS_HEADLESS_LIB ${PROJECT_BASE_NAME}-graphics-headless CACHE INTERNAL "Library containing a minimal headless graphics implementation")
set(PROJECT_GRAPHICS_EGL_LIB ${PROJECT_BASE_NAME}-graphics-egl CACHE INTERNAL "Library containing an OpenGL-based graphics implementation using EGL")
set(PROJECT_GRAPHICS_OPENGL_LIB ${PROJECT_BASE_NAME}-graphics-opengl CACHE INTERNAL "Library containing an OpenGL-based graphics implementation using OpenGL 4.0")

set(PROJECT_INPUT_CORE_LIB ${PROJECT_BASE_NAME}-input-core CACHE INTERNAL "Library containing generic user input support")
set(PROJECT_INPUT_GLFW_LIB ${PROJECT_BASE_NAME}-input-glfw CACHE INTERNAL "Library implementing user input support using GLFW")

set(PROJECT_PHYSICS_PHYSX_LIB ${PROJECT_BASE_NAME}-physics-physx CACHE INTERNAL "Library implementing physics using PhysX")

set(PROJECT_XR_CORE_LIB ${PROJECT_BASE_NAME}-xr-core CACHE INTERNAL "Library containing generic XR/VR support")
set(PROJECT_XR_OPENVR_LIB ${PROJECT_BASE_NAME}-xr-openvr CACHE INTERNAL "Library implementing XR support using OpenVR")

add_subdirectory(core)
add_subdirectory(game)
add_subdirectory(graphics)
add_subdirectory(input)
add_subdirectory(physx)
add_subdirectory(xr)
