add_subdirectory(gui)

target_sources(
    ${PROJECT_LIB}
    PRIVATE
        GameLogic.cc
        GuiManager.cc
        InputManager.cc        
)