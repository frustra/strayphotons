add_subdirectory(components)
add_subdirectory(systems)

target_sources(
    ${PROJECT_LIB}
    PRIVATE
        Components.cc
        Ecs.cc
)
