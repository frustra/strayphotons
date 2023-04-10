add_library(${PROJECT_SCRIPTS_LIB} OBJECT) # Must be OBJECT instead of STATIC so the linker doesn't skip unreferenced files

target_include_directories(${PROJECT_SCRIPTS_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_SCRIPTS_LIB} PUBLIC
    ${PROJECT_CORE_LIB}
    ${PROJECT_GAME_LIB}
)

target_compile_definitions(${PROJECT_SCRIPTS_LIB} PUBLIC
    SP_SCRIPTS_SUPPORT
)

# target_precompile_headers(${PROJECT_SCRIPTS_LIB} REUSE_FROM ${PROJECT_CORE_LIB})

add_subdirectory(scripts)
add_subdirectory(prefabs)
