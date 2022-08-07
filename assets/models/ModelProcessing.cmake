# Function that produces a CMake target to process an OBJ file, including processing its MTL and all its 
# texture files.
# The output is a GLTF file. The GLTF will automatically be rebuild when any of the 
# obj, mtl, or texture files are changed.
function(process_obj_to_gltf)
    cmake_parse_arguments(
        PARAM
        "" # Options
        "TARGET;MODEL;OBJ;MTL;GLTF;NODE_EXE" # Single value args
        "TEXTURES;DEPENDS" # Multi-value args
        ${ARGN}
    )

    add_custom_command(
        COMMAND
            ${PARAM_NODE_EXE} ${PROJECT_SOURCE_DIR}/ext/obj2gltf/bin/obj2gltf.js --secure --checkTransparency --metallicRoughness -i ${PARAM_OBJ} -o ${PARAM_GLTF}
        WORKING_DIRECTORY
            ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT
            ${PARAM_GLTF}
        DEPENDS
            ${PARAM_DEPENDS}
            ${PARAM_OBJ}
            ${PARAM_MTL}
            ${PARAM_TEXTURES}
            ${PROJECT_SOURCE_DIR}/ext/obj2gltf/node_modules
    )

    add_custom_target(${PARAM_MODEL}-gltf DEPENDS ${PARAM_GLTF})
    add_dependencies(${PARAM_TARGET} ${PARAM_MODEL}-gltf)
endfunction()

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
            ${PARAM_NODE_EXE} ${PROJECT_SOURCE_DIR}/ext/gltf-pipeline/bin/gltf-pipeline.js -i ${PARAM_GLTF} -o ${PARAM_GLTF}
        COMMAND
            ${PARAM_NODE_EXE} ${PROJECT_SOURCE_DIR}/ext/gltf-pipeline/bin/gltf-pipeline.js -i ${PARAM_GLTF} -o ${PARAM_GLB}
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
        endif()
    endif()
    
    if(EXISTS "${physics_path}")
        add_custom_command(
            COMMAND
                hull_compiler ${PARAM_MODEL}
            WORKING_DIRECTORY
                ${PROJECT_SOURCE_DIR}/bin
            OUTPUT
                ${PROJECT_SOURCE_DIR}/assets/cache/collision/${PARAM_MODEL}
            DEPENDS
                hull_compiler
                ${physics_path}
        )

        add_custom_target(${PARAM_MODEL}-physics DEPENDS ${PROJECT_SOURCE_DIR}/assets/cache/collision/${PARAM_MODEL})
        add_dependencies(${PARAM_TARGET} ${PARAM_MODEL}-physics)
    endif()
endfunction()

# Top level function that wraps a lot of the functionality required to fully process a model.
# First, it guesses the model name from the folder name.
# Next, it globs any .png or .tga files in the folder and assumes they're textures
# Then it processes the obj and mtl into a gltf.
# Finally, it produces a glb.
function(process_model_folder)
    cmake_parse_arguments(
        PARAM
        "" # Options
        "TARGET;MODEL;NODE_EXE" # Single value args
        "" # Multi-value args
        ${ARGN}
    )

    set(_obj_file ${CMAKE_CURRENT_LIST_DIR}/${PARAM_MODEL}/${PARAM_MODEL}.obj)
    set(_glb_file ${CMAKE_CURRENT_LIST_DIR}/${PARAM_MODEL}/${PARAM_MODEL}.glb)
    set(_gltf_file ${CMAKE_CURRENT_LIST_DIR}/${PARAM_MODEL}/${PARAM_MODEL}.gltf)

    if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/${PARAM_MODEL}/${PARAM_MODEL}.mtl)
        set(_mtl_file ${CMAKE_CURRENT_LIST_DIR}/${PARAM_MODEL}/${PARAM_MODEL}.mtl)
    endif()

    file(GLOB _png_files "${CMAKE_CURRENT_LIST_DIR}/${PARAM_MODEL}/*.png")
    file(GLOB _tga_files "${CMAKE_CURRENT_LIST_DIR}/${PARAM_MODEL}/*.tga")
    
    process_obj_to_gltf(
        TARGET
            ${PARAM_TARGET}
        MODEL
            ${PARAM_MODEL}
        OBJ
            ${_obj_file}
        MTL
            ${_mtl_file}
        GLTF
            ${_gltf_file}
        TEXTURES
            ${_png_files}
            ${_tga_files}
        NODE_EXE
            ${PARAM_NODE_EXE}
    )

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
