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
set(OBJ_MODEL_FOLDERS
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
    spotlight-01
    wall-1x1
    wall-4-corner
    wall-4x3
    wall-4x3-door
)

set(RAW_MODEL_FOLDERS
    sponza
    sponza2
    duck
    station-segment
)

set(RAW_MODELS
    box
    cornell-box-1
    cornell-box-2
    cornell-box-3
    mcube-physx
    mirror
    occluder
    colorfilter
)

foreach(_model ${OBJ_MODEL_FOLDERS})
    process_model_folder(
        TARGET
            models
        MODEL
            ${_model}
        NODE_EXE
            ${NODE_JS_EXE}
    )
endforeach()

foreach(_model ${RAW_MODEL_FOLDERS})
    process_gltf_to_glb(
        TARGET
            models
        MODEL
            ${_model}
        GLTF
            ${CMAKE_CURRENT_LIST_DIR}/${_model}/${_model}.gltf
        GLB
            ${CMAKE_CURRENT_LIST_DIR}/${_model}/${_model}.glb
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

foreach(_model ${RAW_MODELS})
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

# Make the project exe depend on having up to date models
add_dependencies(${PROJECT_COMMON_EXE} models)
