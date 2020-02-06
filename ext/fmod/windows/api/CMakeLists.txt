
# Figure out which lib to use
if (${CMAKE_SIZEOF_VOID_P} EQUAL 4)
    set (_fmod_arch "x86")
elseif(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    set (_fmod_arch "x64")
endif()

# Add fmod lib
## GLOBAL is required to allow visibility of this lib to parent directories
add_library(fmod SHARED IMPORTED GLOBAL)

target_include_directories(
    fmod
    INTERFACE
        core/inc
)


# On Windows, need to find the DLL and .lib file. Annoyingly, these have different names!
# TODO: support debug/release versions using IMPORTED_<LIBTYPE>_<CONFIG>
set_property(
    TARGET
        fmod
    PROPERTY
        IMPORTED_IMPLIB ${CMAKE_CURRENT_LIST_DIR}/core/lib/${_fmod_arch}/fmod_vc.lib
)

set_property(
    TARGET
        fmod
    PROPERTY
        IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/core/lib/${_fmod_arch}/fmod.dll
)

install(
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/core/lib/${_fmod_arch}/fmod.dll
    DESTINATION
        ${CMAKE_INSTALL_BINDIR}
    COMPONENT
        ${PROJECT_INSTALL}
)

# Ensure fmod ends up in the runtime output directory
add_custom_command(
    OUTPUT
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/fmod.dll
    COMMAND   
        ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/core/lib/${_fmod_arch}/fmod.dll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/fmod.dll
)

add_custom_target(
    fmod-runtime-copy
    DEPENDS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/fmod.dll
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
        IMPORTED_IMPLIB ${CMAKE_CURRENT_LIST_DIR}/studio/lib/${_fmod_arch}/fmodstudio_vc.lib
)

set_property(
    TARGET
        fmodstudio
    PROPERTY
        IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/studio/lib/${_fmod_arch}/fmodstudio.dll
)

install(
    FILES 
        ${CMAKE_CURRENT_LIST_DIR}/studio/lib/${_fmod_arch}/fmodstudio.dll
    DESTINATION
        ${CMAKE_INSTALL_BINDIR}
    COMPONENT
        ${PROJECT_INSTALL}
)

# Ensure fmodstudio ends up in the runtime output directory
add_custom_command(
    OUTPUT
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/fmodstudio.dll
    COMMAND   
        ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/studio/lib/${_fmod_arch}/fmodstudio.dll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/fmodstudio.dll
)

add_custom_target(
    fmodstudio-runtime-copy
    DEPENDS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/fmodstudio.dll
)

add_dependencies(fmodstudio fmodstudio-runtime-copy)
