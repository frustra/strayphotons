
target_sources(${PROJECT_CORE_LIB} PRIVATE
    Components.cc
    Ecs.cc
    NamedEntity.cc
    Signals.cc
)

add_subdirectory(components)
