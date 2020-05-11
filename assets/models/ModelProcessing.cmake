# Function that produces a CMake target to process an OBJ file, including processing its MTL and all its 
# texture files.
# The output is a GLTF file. The GLTF will automatically be rebuild when any of the 
# obj, mtl, or texture files are changed.
function(process_obj_to_gltf)
    cmake_parse_arguments(
        PARAM
        "" # Options
        "TARGET;OBJ;MTL;GLTF;NODE_EXE" # Single value args
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
            obj2gltf-install
    )

    add_custom_target(${PARAM_TARGET} DEPENDS ${PARAM_GLTF})
endfunction()

# Function that produces a CMake target to process a GLTF file into a GLB.
# The output is a GLB file. The GLB will automatically be rebuild when the GLTF is updated.
function(process_gltf_to_glb)
    cmake_parse_arguments(
        PARAM
        "" # Options
        "TARGET;GLTF;GLB;NODE_EXE" # Single value args
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
            gltf-pipeline-install
    )

    add_custom_target(${PARAM_TARGET} DEPENDS ${PARAM_GLB})
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
            ${PARAM_TARGET}-gltf
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
            ${PARAM_TARGET}-glb
        GLTF
            ${_gltf_file}
        GLB
            ${_glb_file}
        NODE_EXE
            ${PARAM_NODE_EXE}
    )

    add_custom_target(${PARAM_TARGET})
    add_dependencies(${PARAM_TARGET} ${PARAM_TARGET}-gltf ${PARAM_TARGET}-glb)
endfunction()
