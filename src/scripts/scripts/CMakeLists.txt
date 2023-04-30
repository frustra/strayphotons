target_sources(${PROJECT_SCRIPTS_LIB} PRIVATE
    AudioScripts.cc
    EditorScripts.cc
    Elevator.cc
    EventScripts.cc
    InputScripts.cc
    InteractionScripts.cc
    Life.cc
    LightScripts.cc
    Magnets.cc
    MiscScripts.cc
    PhysicsScripts.cc
    VrHandScript.cc
)

add_executable(fusion_test FusionReactor.cc)
