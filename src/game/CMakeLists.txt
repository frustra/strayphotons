add_subdirectory(systems)

target_sources(${PROJECT_EXE} PRIVATE
    Game.cc
    GameLogic.cc
    Main.cc
)

target_sources(${PROJECT_HEADLESS_EXE} PRIVATE
    Game.cc
    GameLogic.cc
    MainTest.cc
)

target_sources(${PROJECT_TEST_EXE} PRIVATE
    Game.cc
    GameLogic.cc
    MainTest.cc
)
