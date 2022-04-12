
target_sources(${PROJECT_CORE_LIB} PRIVATE
    Components.cc
    Ecs.cc
    EntityRef.cc
    EntityReferenceManager.cc
)

target_precompile_headers(${PROJECT_CORE_LIB} PUBLIC
    Components.hh
    Ecs.hh
    EcsImpl.hh
    EntityRef.hh
)

add_subdirectory(components)
