
target_sources(
    ${PROJECT_LIB}
    PRIVATE
        Console.cc
        Game.cc
        CVar.cc
        PerfTimer.cc
)

target_sources(
    ${PROJECT_EXE}
    PRIVATE
        Main.cc
)