add_executable(sp-ecs-benchmark benchmark.cc)
target_link_libraries(sp-ecs-benchmark ${PROJECT_CORE_LIB} glomerate robin_hood)

# Change glomerate to use robin hood hashing
if(MSVC)
    target_compile_options(
        sp-ecs-benchmark
        PRIVATE
            /FIrobin_hood.h
            -DGLOMERATE_MAP_TYPE=robin_hood::unordered_flat_map)
else()
    # GCC or Clang
    target_compile_options(
        sp-ecs-benchmark
        PRIVATE
            -include robin_hood.h
            -DGLOMERATE_MAP_TYPE=robin_hood::unordered_flat_map)
endif()

# target to run the tests
add_custom_target(
    ${test_type}-tests
    COMMAND sp-ecs-benchmark
    DEPENDS sp-ecs-benchmark
COMMENT "Run ${test_type} tests")
