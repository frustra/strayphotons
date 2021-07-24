
add_library(${PROJECT_PHYSICS_PHYSX_LIB} STATIC)

target_include_directories(${PROJECT_PHYSICS_PHYSX_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_PHYSICS_PHYSX_LIB}
    PRIVATE
        ${PROJECT_CORE_LIB}
        ${PROJECT_INPUT_CORE_LIB}
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

if (SP_DEBUG)
	target_compile_definitions(${PROJECT_PHYSICS_PHYSX_LIB} PUBLIC
		_DEBUG
	)
else()
	target_compile_definitions(${PROJECT_PHYSICS_PHYSX_LIB} PUBLIC
		NDEBUG
	)
endif()

add_subdirectory(physx)
