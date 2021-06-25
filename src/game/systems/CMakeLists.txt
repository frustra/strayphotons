
target_sources(${PROJECT_EXE} PRIVATE
    AnimationSystem.cc
    HumanControlSystem.cc
)

target_sources(${PROJECT_HEADLESS_EXE} PRIVATE
    AnimationSystem.cc
    HumanControlSystem.cc
)

target_sources(${PROJECT_TEST_EXE} PRIVATE
    AnimationSystem.cc
    HumanControlSystem.cc
)
