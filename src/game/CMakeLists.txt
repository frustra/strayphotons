add_library(${PROJECT_GAME_TEST_LIB} INTERFACE)
target_compile_definitions(${PROJECT_GAME_TEST_LIB} INTERFACE SP_TEST_MODE)

add_module_sources(
    Game.cc
    Main.cc
)
