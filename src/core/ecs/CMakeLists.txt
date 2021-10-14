
target_sources(${PROJECT_CORE_LIB} PRIVATE
    Components.cc
    Ecs.cc
    NamedEntity.cc
)

target_precompile_headers(${PROJECT_CORE_LIB} PUBLIC Ecs.hh EcsImpl.hh)

add_subdirectory(components)
