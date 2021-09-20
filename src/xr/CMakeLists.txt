add_library(${PROJECT_XR_CORE_LIB} STATIC)
add_library(${PROJECT_XR_OPENVR_LIB} STATIC)
add_library(${PROJECT_XR_OPENVR_GL_LIB} STATIC)
add_library(${PROJECT_XR_OPENVR_VK_LIB} STATIC)

target_include_directories(${PROJECT_XR_CORE_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_XR_CORE_LIB} PUBLIC
    ${PROJECT_CORE_LIB}
)

target_link_libraries(${PROJECT_XR_OPENVR_LIB} PUBLIC
    ${PROJECT_XR_CORE_LIB}
    ${PROJECT_INPUT_CORE_LIB}
    ${PROJECT_GRAPHICS_CORE_LIB}
    openvr
)

target_link_libraries(${PROJECT_XR_OPENVR_GL_LIB} PUBLIC
    ${PROJECT_XR_OPENVR_LIB}
    ${PROJECT_GRAPHICS_OPENGL_LIB}
)

target_link_libraries(${PROJECT_XR_OPENVR_VK_LIB} PUBLIC
    ${PROJECT_XR_OPENVR_LIB}
    ${PROJECT_GRAPHICS_VULKAN_LIB}
)

target_compile_definitions(${PROJECT_XR_CORE_LIB} PUBLIC
    SP_XR_SUPPORT
)
target_compile_definitions(${PROJECT_XR_OPENVR_LIB} PUBLIC
    SP_XR_SUPPORT_OPENVR
)

add_subdirectory(xr)

add_module_sources(
    XrManager.cc
)
