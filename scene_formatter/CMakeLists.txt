add_executable(scene_formatter
    main.cc
)

target_link_libraries(scene_formatter
    ${PROJECT_CORE_LIB}
    ${PROJECT_GAME_LIB}
    ${PROJECT_SCRIPTS_LIB}
    cxxopts
)

target_include_directories(scene_formatter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
