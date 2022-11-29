
target_sources(${PROJECT_CORE_LIB} PRIVATE
    ComponentMetadata.cc
    Components.cc
    Ecs.cc
    EntityRef.cc
    EntityReferenceManager.cc
    SignalExpression.cc
)

target_precompile_headers(${PROJECT_CORE_LIB} PUBLIC
    ComponentMetadata.hh
    Components.hh
    Ecs.hh
    EcsImpl.hh
    EntityRef.hh
)

add_subdirectory(components)
