#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

##
## When adding a new script, just add it to this list!
##
set(SCRIPT_LIST
    script
    test
)

find_program(CLANG_BIN clang REQUIRED)

foreach(SCRIPT_NAME IN LISTS SCRIPT_LIST)
    file(GLOB SCRIPT_INPUT src/${SCRIPT_NAME}.*)
    set(SCRIPT_OUTPUT ${PROJECT_OUTPUT_DIR}/scripts/${SCRIPT_NAME}.wasm)
    add_custom_command(
        COMMAND ${CLANG_BIN} --target=wasm32
            -O3
            -flto
            -nostdlib
            -DSP_WASM_BUILD=1
            -I${CMAKE_CURRENT_SOURCE_DIR}/inc
            -I${PROJECT_SOURCE_DIR}/src/core
            -Wl,--allow-undefined-file=${CMAKE_CURRENT_SOURCE_DIR}/wasm.syms
            -Wl,--no-entry
            -Wl,--export-all
            -Wl,--lto-O3
            -o ${SCRIPT_OUTPUT}
            ${SCRIPT_INPUT}
        OUTPUT
            ${SCRIPT_OUTPUT}
        DEPENDS
            ${SCRIPT_INPUT}
            ${PROJECT_CORE_LIB}
            inc/ecs.h
    )
    list(APPEND WASM_OUTPUTS ${SCRIPT_OUTPUT})
endforeach()

add_custom_target(compiled-scripts DEPENDS ${WASM_OUTPUTS})
add_dependencies(${PROJECT_COMMON_EXE} compiled-scripts)
