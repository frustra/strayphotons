
target_sources(${PROJECT_CORE_LIB} PRIVATE
    Components.cc
    Ecs.cc
    EntityRef.cc
    EntityReferenceManager.cc
    EventQueue.cc
    ScriptManager.cc
    SignalExpression.cc
    SignalStructAccess.cc
    StringHandle.cc
    StructMetadata.cc
)

target_precompile_headers(${PROJECT_CORE_LIB} PUBLIC
    Ecs.hh
    EcsImpl.hh
    EntityRef.hh
)

add_subdirectory(components)
