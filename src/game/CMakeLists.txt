add_library(${PROJECT_GAME_LIB} STATIC)
add_library(${PROJECT_GAME_TEST_LIB} INTERFACE)

target_include_directories(${PROJECT_GAME_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_GAME_LIB} PUBLIC
    ${PROJECT_CORE_LIB}
    ${PROJECT_INPUT_CORE_LIB}
)

target_compile_definitions(${PROJECT_GAME_TEST_LIB} INTERFACE SP_TEST_MODE)

add_subdirectory(game)

add_module_sources(
    Game.cc
    Main.cc
)
