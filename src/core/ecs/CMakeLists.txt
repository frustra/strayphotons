
target_sources(${PROJECT_CORE_LIB} PRIVATE
    Components.cc
    Ecs.cc
    EntityRef.cc
    ReferenceManager.cc
    EventQueue.cc
    ScriptManager.cc
    SignalExpression.cc
    SignalRef.cc
    SignalStructAccess.cc
    StructMetadata.cc
)

target_precompile_headers(${PROJECT_CORE_LIB} PUBLIC
    Ecs.hh
    EcsImpl.hh
    EntityRef.hh
)

add_subdirectory(components)
