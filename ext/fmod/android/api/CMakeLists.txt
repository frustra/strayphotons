
# Figure out which lib to use
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
    set (_fmod_arch "arm64-v8a")
else()
    if (${CMAKE_SIZEOF_VOID_P} EQUAL 4)
        set (_fmod_arch "x86")
    elseif(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
        set (_fmod_arch "x86_64")
    endif()
endif()

# Add fmod lib
## GLOBAL is required to allow visibility of this lib to parent directories
add_library(fmod SHARED IMPORTED GLOBAL)

target_include_directories(
    fmod
    INTERFACE
        core/inc
)

set_property(
    TARGET
        fmod
    PROPERTY
        IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/core/lib/${_fmod_arch}/libfmod.so
)

install(
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/core/lib/${_fmod_arch}/libfmod.so
    TYPE
        BIN
    COMPONENT
        ${PROJECT_INSTALL}
)

# Ensure fmod ends up in the runtime output directory
add_custom_command(
    OUTPUT
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libfmod.so
    COMMAND   
        ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/core/lib/${_fmod_arch}/libfmod.so ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libfmod.so
)

add_custom_target(
    fmod-runtime-copy
    DEPENDS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libfmod.so
)

add_dependencies(fmod fmod-runtime-copy)

# Add fmodstudio lib
## GLOBAL is required to allow visibility of this lib to parent directories
add_library(fmodstudio SHARED IMPORTED GLOBAL)

target_include_directories(
    fmodstudio
    INTERFACE
        studio/inc
)

# On Windows, need to find the DLL and .lib file. Annoyingly, these have different names!
# TODO: support debug/release versions using IMPORTED_<LIBTYPE>_<CONFIG>
set_property(
    TARGET
        fmodstudio
    PROPERTY
        IMPORTED_IMPLIB ${CMAKE_CURRENT_LIST_DIR}/studio/lib/${_fmod_arch}/libfmodstudio.so
)

set_property(
    TARGET
        fmodstudio
    PROPERTY
        IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/studio/lib/${_fmod_arch}/libfmodstudio.so
)

install(
    FILES 
        ${CMAKE_CURRENT_LIST_DIR}/studio/lib/${_fmod_arch}/libfmodstudio.so
    TYPE
        BIN
    COMPONENT
        ${PROJECT_INSTALL}
)

# Ensure fmodstudio ends up in the runtime output directory
add_custom_command(
    OUTPUT
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libfmodstudio.so
    COMMAND   
        ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/studio/lib/${_fmod_arch}/libfmodstudio.so ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libfmodstudio.so
)

add_custom_target(
    fmodstudio-runtime-copy
    DEPENDS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libfmodstudio.so
)

add_dependencies(fmodstudio fmodstudio-runtime-copy)
