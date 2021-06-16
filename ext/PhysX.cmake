if (${CMAKE_SIZEOF_VOID_P} EQUAL 4)
    set(PX_OUTPUT_ARCH x86)
elseif(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    set(PX_OUTPUT_ARCH x86_64)
endif()

set(PHYSX_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/physx/physx")

set(PX_OUTPUT_LIB_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(PX_OUTPUT_BIN_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

set(PX_GENERATE_STATIC_LIBRARIES OFF CACHE BOOL "Disable PhysX static lib generation" FORCE)
set(NV_USE_GAMEWORKS_OUTPUT_DIRS ON CACHE BOOL "Use old PhysX directory structure" FORCE)

set(ENV{PM_PATHS} ${CMAKE_CURRENT_LIST_DIR}/physx/externals/opengl-linux)
set(ENV{PM_CMakeModules_PATH} ${CMAKE_CURRENT_LIST_DIR}/physx/externals/cmakemodules)
set(ENV{PM_PxShared_PATH} ${CMAKE_CURRENT_LIST_DIR}/physx/pxshared)

if (APPLE)

    # TODO: harmonize with top level cmake flags
    set(TARGET_BUILD_PLATFORM mac)
    set(OUR_PHYSX_CXX_FLAGS_FOR_APPLE "-faligned-allocation -Wno-atomic-implicit-seq-cst")

    set(PHYSX_CXX_FLAGS_DEBUG "-O0 -g ${OUR_PHYSX_CXX_FLAGS_FOR_APPLE}" CACHE INTERNAL "" FORCE)
    set(PHYSX_CXX_FLAGS_CHECKED "-O3 -g ${OUR_PHYSX_CXX_FLAGS_FOR_APPLE}" CACHE INTERNAL "" FORCE)
    set(PHYSX_CXX_FLAGS_PROFILE "-O3 -g ${OUR_PHYSX_CXX_FLAGS_FOR_APPLE}" CACHE INTERNAL "" FORCE)
    set(PHYSX_CXX_FLAGS_RELEASE "-O3 -g ${OUR_PHYSX_CXX_FLAGS_FOR_APPLE}" CACHE INTERNAL "" FORCE)
    set(NV_FORCE_64BIT_SUFFIX TRUE)
    set(NV_FORCE_32BIT_SUFFIX FALSE)

elseif (UNIX)

    set(TARGET_BUILD_PLATFORM linux)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "11.0.0")
            add_compile_options(-Wno-error=nonnull -Wno-error=mismatched-new-delete -Wno-error=stringop-overread)
        endif()
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "10.0.0")
            add_compile_options(-Wno-error=array-bounds)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "12.0.0")
            add_compile_options(-Wno-suggest-override -Wno-suggest-destructor-override)
        endif()
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "11.0.0")
            add_compile_options(-Wno-dtor-name)
        endif()
    endif()

    set(CMAKE_SYSTEM_NAME Linux)
    set(CMAKE_SYSTEM_VERSION 1)
    set(CMAKE_SYSROOT ${LINUX_ROOT})
    set(CMAKE_LIBRARY_ARCHITECTURE x86_64-linux-gnu)

elseif (MSVC)

    set(TARGET_BUILD_PLATFORM windows)
    set(NV_USE_STATIC_WINCRT FALSE)

    if (SP_DEBUG)
        set(NV_USE_DEBUG_WINCRT TRUE CACHE BOOL "Force PhysX to use Debug WINCRT" FORCE)
    else()
        set(NV_USE_DEBUG_WINCRT FALSE CACHE BOOL "Force PhysX to use Debug WINCRT" FORCE)
    endif()

    # Help PhysX find the "typeinfo.h" helper file, since it uses an outdated #include on Win32
    include_directories(
        typeinfo
    )

endif()

add_subdirectory(physx/physx/compiler/public)

# Need to copy files out of the PhysX build folder into the real output folder
if (WIN32)

    set(_physx_targets
        PhysX
        PhysXCharacterKinematic
        PhysXCommon
        PhysXCooking
        PhysXExtensions
        PhysXFoundation
        PhysXPvdSDK
    )

    foreach(_physx_target ${_physx_targets})
        if (TARGET ${_physx_target})

            add_custom_command(
                OUTPUT
                    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_physx_target}-uptodate
                COMMAND
                    ${CMAKE_COMMAND} -E touch ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_physx_target}-uptodate
                COMMAND
                    ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${_physx_target}> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<TARGET_FILE_NAME:${_physx_target}>
                DEPENDS
                    ${_physx_target}
                    $<TARGET_FILE:${_physx_target}>
            )

            add_custom_target(
                ${_physx_target}-dll-copy 
                DEPENDS
                    ${_physx_target}
                    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_physx_target}-uptodate
            )

            add_dependencies(${PROJECT_LIB} ${_physx_target}-dll-copy)

        endif()
    endforeach()

endif()
