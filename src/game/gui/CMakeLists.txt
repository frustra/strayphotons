target_sources(
    ${PROJECT_LIB}
    PRIVATE
        DebugGuiManager.cc
        MenuGuiManager.cc
)

target_sources(
    ${PROJECT_EXE}
    PRIVATE
        GuiManager.cc
)

target_sources(
    ${PROJECT_TEST_EXE}
    PRIVATE
        GuiManager.cc
)
