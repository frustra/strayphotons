#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(openvr SHARED IMPORTED GLOBAL)

target_include_directories(
    openvr
    INTERFACE
        ./openvr/headers
)

if (WIN32)

    if (${CMAKE_SIZEOF_VOID_P} EQUAL 4)
        set (_openvr_win_arch "win32")
    elseif(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
        set (_openvr_win_arch "win64")
    endif()

    set_property(
        TARGET
            openvr
        PROPERTY
            IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/openvr/bin/${_openvr_win_arch}/openvr_api.dll
    )

    set_property(
        TARGET
            openvr
        PROPERTY
            IMPORTED_IMPLIB ${CMAKE_CURRENT_LIST_DIR}/openvr/lib/${_openvr_win_arch}/openvr_api.lib
    )

    install(
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/openvr/bin/${_openvr_win_arch}/openvr_api.dll
        DESTINATION
            ${CMAKE_INSTALL_BINDIR}
        COMPONENT
            ${PROJECT_INSTALL}
    )

    # Ensure openvr ends up in the runtime output directory
    add_custom_command(
        OUTPUT
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/openvr_api.dll
        COMMAND   
            ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/openvr/bin/${_openvr_win_arch}/openvr_api.dll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/openvr_api.dll
    )

    # Ensure openvr pdb ends up in the runtime output directory
    add_custom_command(
        OUTPUT
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/openvr_api.pdb
        COMMAND   
            ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/openvr/bin/${_openvr_win_arch}/openvr_api.pdb ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/openvr_api.pdb
    )

    add_custom_target(
        openvr-runtime-copy
        DEPENDS
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/openvr_api.dll
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/openvr_api.pdb
    )

    add_dependencies(openvr openvr-runtime-copy)

elseif(UNIX)

    if (${CMAKE_SIZEOF_VOID_P} EQUAL 4)
        message(FATAL_ERROR "OpenVR only supports 64-bit compiles on Linux")
    endif()

    set_property(
        TARGET
            openvr
        PROPERTY
            IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/openvr/lib/linux64/libopenvr_api.so
    )

    install(
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/openvr/bin/linux64/libopenvr_api.so
        TYPE
            BIN
        COMPONENT
            ${PROJECT_INSTALL}
    )

    # Ensure openvr ends up in the runtime output directory
    add_custom_command(
        OUTPUT
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libopenvr_api.so
        COMMAND   
            ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/openvr/bin/linux64/libopenvr_api.so ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libopenvr_api.so
    )

    add_custom_target(
        openvr-runtime-copy
        DEPENDS
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libopenvr_api.so
    )

    add_dependencies(openvr openvr-runtime-copy)

endif()
