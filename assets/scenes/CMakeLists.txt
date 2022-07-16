if(SP_NO_MODEL_COMPILE)
    message(WARNING "!!! Model compiling is disabled !!!")
    return()
endif()

# Make sure Node.js is installed
find_program(NODE_JS_EXE node)

if (${NODE_JS_EXE} STREQUAL NODE_JS_EXE-NOTFOUND)
    message(FATAL_ERROR "Node.js is not installed. It is required for model compiling.")
endif()

##
## When adding a new .scene file, just add it to this list!
##
set(SCENES_INPUT
    sponza
)

list(TRANSFORM SCENES_INPUT PREPEND "${CMAKE_CURRENT_LIST_DIR}/")

set(SCENES_OUTPUT ${SCENES_INPUT})

list(TRANSFORM SCENES_INPUT APPEND ".scene")
list(TRANSFORM SCENES_OUTPUT APPEND ".json")

add_custom_command(
    COMMAND
        ${NODE_JS_EXE} ${PROJECT_SOURCE_DIR}/assets/util/render-scenes.js
    OUTPUT
        ${SCENES_OUTPUT}
    DEPENDS
        ${SCENES_INPUT}
    WORKING_DIRECTORY
        ${PROJECT_SOURCE_DIR}/assets/util
)

add_custom_target(compiled-scenes DEPENDS ${SCENES_OUTPUT})
add_dependencies(${PROJECT_COMMON_EXE} compiled-scenes)
