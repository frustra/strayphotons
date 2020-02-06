
# Figure out which lib to use
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
    set (_fmod_arch "armhf")
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
        IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/core/lib/${_fmod_arch}/libfmod.so.11.6
)

install(
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/core/lib/${_fmod_arch}/libfmod.so.11.6
    TYPE
        BIN
    COMPONENT
        ${PROJECT_INSTALL}
)

# Ensure fmod ends up in the runtime output directory
add_custom_command(
    OUTPUT
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libfmod.so.11.6
    COMMAND   
        ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/core/lib/${_fmod_arch}/libfmod.so.11.6 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libfmod.so.11.6
)

add_custom_target(
    fmod-runtime-copy
    DEPENDS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libfmod.so.11.6
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

set_property(
    TARGET
        fmodstudio
    PROPERTY
        IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/studio/lib/${_fmod_arch}/libfmodstudio.so.11.6
)

install(
    FILES 
        ${CMAKE_CURRENT_LIST_DIR}/studio/lib/${_fmod_arch}/libfmodstudio.so.11.6
    TYPE
        BIN
    COMPONENT
        ${PROJECT_INSTALL}
)

# Ensure fmodstudio ends up in the runtime output directory
add_custom_command(
    OUTPUT
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libfmodstudio.so.11.6
    COMMAND   
        ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/studio/lib/${_fmod_arch}/libfmodstudio.so.11.6 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libfmodstudio.so.11.6
)

add_custom_target(
    fmodstudio-runtime-copy
    DEPENDS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libfmodstudio.so.11.6
)

add_dependencies(fmodstudio fmodstudio-runtime-copy)
