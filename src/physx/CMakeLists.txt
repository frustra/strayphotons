
add_library(${PROJECT_PHYSICS_PHYSX_LIB} OBJECT)

target_include_directories(${PROJECT_PHYSICS_PHYSX_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_PHYSICS_PHYSX_LIB}
    PRIVATE
        ${PROJECT_CORE_LIB}
        ${PROJECT_GRAPHICS_CORE_LIB}
        vhacd
    PUBLIC
        PhysX
        PhysXCharacterKinematic
        PhysXCommon
        PhysXCooking
        PhysXExtensions
        PhysXFoundation
        PhysXPvdSDK
)

target_compile_definitions(${PROJECT_PHYSICS_PHYSX_LIB}
    PRIVATE
        PX_PHYSX_STATIC_LIB
    PUBLIC
        SP_PHYSICS_SUPPORT_PHYSX
)

target_precompile_headers(${PROJECT_PHYSICS_PHYSX_LIB} REUSE_FROM ${PROJECT_CORE_LIB})

add_subdirectory(physx)
