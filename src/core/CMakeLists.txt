
target_sources(
    ${PROJECT_LIB}
    PRIVATE
        Console.cc
        PerfTimer.cc
)

target_sources(
    ${PROJECT_EXE}
    PRIVATE
        Main.cc
        Game.cc
)


target_sources(
    ${PROJECT_TEST_EXE}
    PRIVATE
        MainTest.cc
        Game.cc
)
