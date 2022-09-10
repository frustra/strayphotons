
add_library(${PROJECT_PHYSICS_PHYSX_LIB} STATIC)
add_library(${PROJECT_PHYSICS_COOKING_LIB} STATIC)

target_include_directories(${PROJECT_PHYSICS_PHYSX_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(${PROJECT_PHYSICS_COOKING_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_PHYSICS_PHYSX_LIB}
    PRIVATE
        ${PROJECT_CORE_LIB}
        ${PROJECT_GAME_LIB}
        ${PROJECT_PHYSICS_COOKING_LIB}
    PUBLIC
        PhysX
        PhysXCharacterKinematic
        PhysXPvdSDK
)
target_link_libraries(${PROJECT_PHYSICS_COOKING_LIB}
    PUBLIC
        ${PROJECT_CORE_LIB}
        vhacd
        PhysXCooking
        PhysXExtensions
)

target_compile_definitions(${PROJECT_PHYSICS_PHYSX_LIB}
    PUBLIC
        SP_PHYSICS_SUPPORT_PHYSX
)

target_precompile_headers(${PROJECT_PHYSICS_PHYSX_LIB} REUSE_FROM ${PROJECT_CORE_LIB})

add_subdirectory(cooking)
add_subdirectory(physx)
