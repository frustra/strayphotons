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

# Custom commands to make sure gltf-pipeline and obj2gltf dependencies are installed

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
set(GLTF_MODEL_FOLDERS
    01-outside
    airlock
    cardboard-box
    ceiling-3x3
    dodecahedron
    door-frame
    door-panel
    duck
    ground-3x3
    level1
    mcube
    parking-block
    pbr-cube
    sensor
    sensor-plate-dark
    sensor-plate-light
    spotlight-01
    sponza
    sponza2
    station-segment
    wall-4-corner
    wall-4x3
    wall-4x3-door
)

set(GLTF_MODELS
    box
    sphere
    cornell-box-1
    cornell-box-2
    cornell-box-3
    mcube-physx
    mirror
    occluder
    colorfilter
)

set(GLB_MODELS
    articulating-arm
    breadboard_corner
    breadboard_edge
    breadboard_surface
    extrusion
    laser-plug-mount
    laser-plug
    laser-socket
    octagon
    optic-plug
    optic-socket
    optic-tube-3mm-flat-one-side
    optic-tube-end-5mm
    red_led
    round-mirror-mounted
)

foreach(_model ${GLTF_MODEL_FOLDERS})
    process_model_folder(
        TARGET
            models
        MODEL
            ${_model}
        NODE_EXE
            ${NODE_JS_EXE}
    )
endforeach()

foreach(_model ${GLTF_MODELS})
    process_gltf_to_glb(
        TARGET
            models
        MODEL
            ${_model}
        GLTF
            ${CMAKE_CURRENT_LIST_DIR}/${_model}.gltf
        GLB
            ${CMAKE_CURRENT_LIST_DIR}/${_model}.glb
        NODE_EXE
            ${NODE_JS_EXE}
    )
    
    update_physics_cache(
        TARGET
            models
        MODEL
            ${_model}
    )
endforeach()

foreach(_model ${GLB_MODELS})
    update_physics_cache(
        TARGET
            models
        MODEL
            ${_model}
    )
endforeach()

# Make the project exe depend on having up to date models
add_dependencies(${PROJECT_COMMON_EXE} models)
