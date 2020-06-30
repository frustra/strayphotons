add_subdirectory(gui)
add_subdirectory(input)

target_sources(
    ${PROJECT_LIB}
    PRIVATE
        GameLogic.cc
)
