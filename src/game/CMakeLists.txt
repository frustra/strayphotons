add_subdirectory(gui)
add_subdirectory(systems)

target_sources(${PROJECT_LIB} PRIVATE
    GameLogic.cc
    Game.cc
)

target_sources(${PROJECT_EXE} PRIVATE
    Main.cc
)

# target_sources(${PROJECT_HEADLESS_EXE} PRIVATE
#     Main.cc
# )

target_sources(${PROJECT_TEST_EXE} PRIVATE
    MainTest.cc
)
