add_executable(docs_generator
    main.cc
)

target_link_libraries(docs_generator
    ${PROJECT_CORE_LIB}
    ${PROJECT_GAME_LIB}
    ${PROJECT_SCRIPTS_LIB}
    cxxopts
)

target_precompile_headers(docs_generator REUSE_FROM ${PROJECT_CORE_LIB})

target_include_directories(docs_generator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
