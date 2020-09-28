add_executable(sp-ecs-benchmark ecs.cc)
target_link_libraries(sp-ecs-benchmark ${PROJECT_LIB} glomerate)

# target to run the tests
add_custom_target(
    ${test_type}-tests
    COMMAND sp-ecs-benchmark
    DEPENDS sp-ecs-benchmark
COMMENT "Run ${test_type} tests")
