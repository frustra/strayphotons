set(PROJECT_CORE_LIB ${PROJECT_BASE_NAME}-core CACHE INTERNAL "Library containing core components such as the ECS and asset loading")

set(PROJECT_GRAPHICS_CORE_LIB ${PROJECT_BASE_NAME}-graphics-core CACHE INTERNAL "Library containing context-independent graphics code")
set(PROJECT_GRAPHICS_EGL_LIB ${PROJECT_BASE_NAME}-graphics-egl CACHE INTERNAL "Library containing an OpenGL-based graphics implementation using EGL")
set(PROJECT_GRAPHICS_OPENGL_LIB ${PROJECT_BASE_NAME}-graphics-opengl CACHE INTERNAL "Library containing an OpenGL-based graphics implementation using OpenGL 4.0")
set(PROJECT_GRAPHICS_VULKAN_LIB ${PROJECT_BASE_NAME}-graphics-vulkan CACHE INTERNAL "Library containing a Vulkan-based graphics implementation")

set(PROJECT_INPUT_CORE_LIB ${PROJECT_BASE_NAME}-input-core CACHE INTERNAL "Library containing generic user input support")
set(PROJECT_INPUT_GLFW_LIB ${PROJECT_BASE_NAME}-input-glfw CACHE INTERNAL "Library implementing user input support using GLFW")

set(PROJECT_PHYSICS_PHYSX_LIB ${PROJECT_BASE_NAME}-physics-physx CACHE INTERNAL "Library implementing physics using PhysX")

set(PROJECT_XR_CORE_LIB ${PROJECT_BASE_NAME}-xr-core CACHE INTERNAL "Library containing generic XR/VR support")
set(PROJECT_XR_OPENVR_LIB ${PROJECT_BASE_NAME}-xr-openvr CACHE INTERNAL "Library implementing XR support using OpenVR")

set(PROJECT_GAME_TEST_LIB ${PROJECT_BASE_NAME}-game-test CACHE INTERNAL "Interface library to enable CLI test mode")

include(${PROJECT_SOURCE_DIR}/cmake/ModuleConfig.cmake)

# Default desktop build
# add_module_configuration(
#     NAME sp
#     LINK_LIBRARIES
#         ${PROJECT_CORE_LIB}
#         ${PROJECT_GRAPHICS_OPENGL_LIB}
#         ${PROJECT_INPUT_GLFW_LIB}
#         ${PROJECT_PHYSICS_PHYSX_LIB}
# )

# Vulkan desktop build
add_module_configuration(
	NAME sp-vk
	LINK_LIBRARIES
		${PROJECT_CORE_LIB}
		${PROJECT_GRAPHICS_VULKAN_LIB}
		${PROJECT_INPUT_GLFW_LIB}
		${PROJECT_PHYSICS_PHYSX_LIB}
        ${PROJECT_XR_OPENVR_LIB}
)

# EGL-based executable used for CI testing purposes
# add_module_configuration(
#     NAME sp-test
#     LINK_LIBRARIES
#         ${PROJECT_GAME_TEST_LIB}
#         ${PROJECT_CORE_LIB}
#         ${PROJECT_GRAPHICS_EGL_LIB}
#         ${PROJECT_INPUT_GLFW_LIB}
# )

# # Headless test executable
# add_module_configuration(
#     NAME sp-headless
#     LINK_LIBRARIES
#         ${PROJECT_CORE_LIB}
#         ${PROJECT_INPUT_CORE_LIB}
#         ${PROJECT_PHYSICS_PHYSX_LIB}
# )

# # Minimal cli interface
# add_module_configuration(
#     NAME sp-cli
#     LINK_LIBRARIES
#         ${PROJECT_GAME_TEST_LIB}
#         ${PROJECT_CORE_LIB}
# )

add_subdirectory(core)
add_subdirectory(game)
add_subdirectory(graphics)
add_subdirectory(input)
add_subdirectory(physx)
add_subdirectory(xr)
