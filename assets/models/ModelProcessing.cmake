# Function that produces a CMake target to process a GLTF file into a GLB.
# The output is a GLB file. The GLB will automatically be rebuild when the GLTF is updated.
function(process_gltf_to_glb)
    cmake_parse_arguments(
        PARAM
        "" # Options
        "TARGET;MODEL;GLTF;GLB;NODE_EXE" # Single value args
        "DEPENDS" # Multi-value args
        ${ARGN}
    )

    add_custom_command(
        COMMAND
            ${CMAKE_COMMAND} -E env NODE_ENV=production ${PARAM_NODE_EXE} ${PROJECT_SOURCE_DIR}/ext/gltf-pipeline/bin/gltf-pipeline.js -i ${PARAM_GLTF} -o ${PARAM_GLB}
        WORKING_DIRECTORY
            ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT
            ${PARAM_GLB}
        DEPENDS
            ${PARAM_DEPENDS}
            ${PARAM_GLTF}
            ${PROJECT_SOURCE_DIR}/ext/gltf-pipeline/node_modules
    )

    add_custom_target(${PARAM_MODEL}-glb DEPENDS ${PARAM_GLB})
    add_dependencies(${PARAM_TARGET} ${PARAM_MODEL}-glb)
endfunction()

# This function will update the physics collision cache for the specified model if a physics.json is present.
function(update_physics_cache)
    cmake_parse_arguments(
        PARAM
        "" # Options
        "TARGET;MODEL" # Single value args
        "DEPENDS" # Multi-value args
        ${ARGN}
    )

    set(physics_path "${PROJECT_SOURCE_DIR}/assets/models/${PARAM_MODEL}/${PARAM_MODEL}.physics.json")
    if(NOT EXISTS "${physics_path}")
        set(physics_path "${PROJECT_SOURCE_DIR}/assets/models/${PARAM_MODEL}/physics.json")
        if(NOT EXISTS "${physics_path}")
            set(physics_path "${PROJECT_SOURCE_DIR}/assets/models/${PARAM_MODEL}.physics.json")
            if(NOT EXISTS "${physics_path}")
                set(physics_path "")
            endif()
        endif()
    endif()

    set(model_path "${PROJECT_SOURCE_DIR}/assets/models/${PARAM_MODEL}/${PARAM_MODEL}.glb")
    if(NOT EXISTS "${model_path}")
        set(model_path "${PROJECT_SOURCE_DIR}/assets/models/${PARAM_MODEL}.glb")
        if(NOT EXISTS "${model_path}")
            set(model_path "")
        endif()
    endif()
    
    add_custom_command(
        COMMAND
            hull_compiler ${PARAM_MODEL}
        WORKING_DIRECTORY
            ${PROJECT_SOURCE_DIR}/bin
        OUTPUT
            "${PROJECT_SOURCE_DIR}/assets/cache/collision/${PARAM_MODEL}"
        DEPENDS
            hull_compiler
            ${model_path}
            ${physics_path}
    )

    add_custom_target(${PARAM_MODEL}-physics DEPENDS "${PROJECT_SOURCE_DIR}/assets/cache/collision/${PARAM_MODEL}")
    add_dependencies(${PARAM_TARGET} ${PARAM_MODEL}-physics)
endfunction()

# This function processes gltf models into glb models for faster load times, and precaches the physics collision hulls.
function(process_model_folder)
    cmake_parse_arguments(
        PARAM
        "" # Options
        "TARGET;MODEL;NODE_EXE" # Single value args
        "" # Multi-value args
        ${ARGN}
    )

    set(_gltf_file ${CMAKE_CURRENT_LIST_DIR}/${PARAM_MODEL}/${PARAM_MODEL}.gltf)
    set(_glb_file ${CMAKE_CURRENT_LIST_DIR}/${PARAM_MODEL}/${PARAM_MODEL}.glb)

    process_gltf_to_glb(
        TARGET
            ${PARAM_TARGET}
        MODEL
            ${PARAM_MODEL}
        GLTF
            ${_gltf_file}
        GLB
            ${_glb_file}
        NODE_EXE
            ${PARAM_NODE_EXE}
    )

    update_physics_cache(
        TARGET
            ${PARAM_TARGET}
        MODEL
            ${PARAM_MODEL}
    )
endfunction()
