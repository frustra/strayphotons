##
## When adding a new script, just add it to this list!
##
set(SCRIPT_LIST
    script
    test
)

foreach(SCRIPT_NAME IN LISTS SCRIPT_LIST)
    file(GLOB SCRIPT_INPUT src/${SCRIPT_NAME}.*)
    set(SCRIPT_OUTPUT ${PROJECT_OUTPUT_DIR}/scripts/${SCRIPT_NAME}.wasm)
    add_custom_command(
        COMMAND clang --target=wasm32
            -O3
            -flto
            -nostdlib
            -I${CMAKE_CURRENT_SOURCE_DIR}/inc
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
            inc/ecs.h
    )
    list(APPEND WASM_OUTPUTS ${SCRIPT_OUTPUT})
endforeach()

add_custom_target(compiled-scripts DEPENDS ${WASM_OUTPUTS})
add_dependencies(${PROJECT_COMMON_EXE} compiled-scripts)
