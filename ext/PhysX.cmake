set(PHYSX_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/physx/physx")

set(PX_OUTPUT_LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/physx")
set(PX_OUTPUT_BIN_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

set(PX_GENERATE_STATIC_LIBRARIES ON CACHE BOOL "Enable static library generation" FORCE)
set(NV_USE_GAMEWORKS_OUTPUT_DIRS ON CACHE BOOL "Use newer output structure" FORCE)

set(ENV{PM_CMakeModules_PATH} ${CMAKE_CURRENT_LIST_DIR}/physx/externals/cmakemodules)
set(ENV{PM_PxShared_PATH} ${CMAKE_CURRENT_LIST_DIR}/physx/pxshared)

if(UNIX)

    set(TARGET_BUILD_PLATFORM linux)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "11.0.0")
            add_compile_options(-Wno-nonnull -Wno-mismatched-new-delete -Wno-stringop-overread)
        endif()
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "10.0.0")
            add_compile_options(-Wno-restrict -Wno-array-bounds -Wno-class-memaccess)
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

elseif(WIN32)

    set(TARGET_BUILD_PLATFORM windows)
    set(NV_USE_STATIC_WINCRT ON CACHE BOOL "Enable PhysX static lib generation" FORCE)

    if(SP_DEBUG)
        set(NV_USE_DEBUG_WINCRT TRUE CACHE BOOL "Force PhysX to use Debug WINCRT" FORCE)
    else()
        set(NV_USE_DEBUG_WINCRT FALSE CACHE BOOL "Force PhysX to use Debug WINCRT" FORCE)
    endif()

endif()

add_subdirectory(physx/physx/compiler/public)
