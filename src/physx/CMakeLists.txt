
target_sources(${PROJECT_EXE} PRIVATE
    ConvexHull.cc
    PhysxManager.cc
)

target_sources(${PROJECT_HEADLESS_EXE} PRIVATE
    ConvexHull.cc
    PhysxManager.cc
)

target_sources(${PROJECT_TEST_EXE} PRIVATE
    ConvexHull.cc
    PhysxManager.cc
)
