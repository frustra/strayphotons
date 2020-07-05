
add_subdirectory(assets)
add_subdirectory(core)
add_subdirectory(ecs)
add_subdirectory(game)
add_subdirectory(graphics)
add_subdirectory(physx)
add_subdirectory(shaders)
add_subdirectory(xr)
add_subdirectory(network)

target_sources(
    ${PROJECT_LIB}
    PRIVATE
        Common.cc
)
