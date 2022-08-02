add_executable(hull_compiler
    main.cc
)

target_link_libraries(hull_compiler
    ${PROJECT_CORE_LIB}
    ${PROJECT_PHYSICS_PHYSX_LIB}
    vhacd
    cxxopts
)

target_include_directories(hull_compiler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
