add_library(${PROJECT_XR_CORE_LIB} INTERFACE)
add_library(${PROJECT_XR_OPENVR_LIB} STATIC)

target_include_directories(${PROJECT_XR_CORE_LIB} INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_XR_CORE_LIB} INTERFACE
    ${PROJECT_CORE_LIB}
)

target_link_libraries(${PROJECT_XR_OPENVR_LIB} PUBLIC
    ${PROJECT_XR_CORE_LIB}
    ${PROJECT_GAME_LIB}
    ${PROJECT_GRAPHICS_CORE_LIB}
    openvr
)

target_compile_definitions(${PROJECT_XR_CORE_LIB} INTERFACE
    SP_XR_SUPPORT
)
target_compile_definitions(${PROJECT_XR_OPENVR_LIB} PUBLIC
    SP_XR_SUPPORT_OPENVR
)

target_precompile_headers(${PROJECT_XR_CORE_LIB} REUSE_FROM ${PROJECT_CORE_LIB})
target_precompile_headers(${PROJECT_XR_OPENVR_LIB} REUSE_FROM ${PROJECT_CORE_LIB})

add_subdirectory(xr)

add_module_sources(
    XrManager.cc
)
