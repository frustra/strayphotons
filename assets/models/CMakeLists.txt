if(SP_NO_MODEL_COMPILE)
    message(WARNING "!!! Model compiling is disabled !!!")
    return()
endif()

include(ModelProcessing.cmake)

# Make sure Node.js and NPM are installed
find_program(NODE_JS_EXE node)
find_program(NPM_EXE npm)

if (${NODE_JS_EXE} STREQUAL NODE_JS_EXE-NOTFOUND)
    message(FATAL_ERROR "Node.js is not installed. It is required for model compiling.")
endif()

if (${NPM_EXE} STREQUAL NPM_EXE-NOTFOUND)
    message(FATAL_ERROR "NPM is not installed. It is required for model compiling.")
endif()

# Custom commands to make sure gltf-pipeline and obj2gltf are installed

add_custom_command(
    COMMAND
        ${NPM_EXE} install
    OUTPUT
        ${PROJECT_SOURCE_DIR}/ext/obj2gltf/node_modules
    WORKING_DIRECTORY
        ${PROJECT_SOURCE_DIR}/ext/obj2gltf
)

add_custom_command(
    COMMAND
        ${NPM_EXE} install
    OUTPUT
        ${PROJECT_SOURCE_DIR}/ext/gltf-pipeline/node_modules
    WORKING_DIRECTORY
        ${PROJECT_SOURCE_DIR}/ext/gltf-pipeline
)

add_custom_target(models)

# Add all the model directories
set(MODEL_FOLDERS
    01-outside
    airlock
    cardboard-box
    ceiling-3x3
    dodecahedron
    door-frame
    door-frame-1.5x2
    door-hex-1x2
    door-hex-frame-1x2
    door-panel
    door-panel-l-1.5x2
    door-panel-r-1.5x2
    # duck ## Do not process, this model comes from a known source
    ground-1x1x1
    ground-3x3
    ledge-caged-1x0.5
    level1
    mcube
    parking-block
    pbr-cube
    sensor
    sensor-plate-dark
    sensor-plate-light
    # sponza ## Do not process, this model comes from a known source
    spotlight-01
    wall-1x1
    wall-4-corner
    wall-4x3
    wall-4x3-door
)

foreach(_model ${MODEL_FOLDERS})
    process_model_folder(
        TARGET
            ${_model}
        MODEL
            ${_model}
        NODE_EXE
            ${NODE_JS_EXE}
    )

    add_dependencies(models ${_model})
endforeach()

set(RAW_MODELS
    box
    cornell-box-1
    cornell-box-2
    cornell-box-3
    mcube-physx
    mirror
    occluder
)

foreach(_model ${RAW_MODELS})
    process_gltf_to_glb(
        TARGET
            ${_model}-gltf
        GLTF
            ${CMAKE_CURRENT_LIST_DIR}/${_model}.gltf
        GLB
            ${CMAKE_CURRENT_LIST_DIR}/${_model}.glb
        NODE_EXE
            ${NODE_JS_EXE}
    )

    add_dependencies(models ${_model}-gltf)
endforeach()

# Process duck and sponza specially
process_gltf_to_glb(
    TARGET
        sponza-gltf
    GLTF
        ${CMAKE_CURRENT_LIST_DIR}/sponza/sponza.gltf
    GLB
        ${CMAKE_CURRENT_LIST_DIR}/sponza/sponza.glb
    NODE_EXE
        ${NODE_JS_EXE}
)
add_dependencies(models sponza-gltf)

process_gltf_to_glb(
    TARGET
        duck-gltf
    GLTF
        ${CMAKE_CURRENT_LIST_DIR}/duck/duck.gltf
    GLB
        ${CMAKE_CURRENT_LIST_DIR}/duck/duck.glb
    NODE_EXE
        ${NODE_JS_EXE}
)
add_dependencies(models duck-gltf)

# Make the project exe depend on having up to date models
add_dependencies(${PROJECT_EXE} models)

