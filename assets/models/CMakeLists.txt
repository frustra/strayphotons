# Add all the model names to process collision caches
set(GLTF_MODELS
    01-outside
    airlock
    articulating-arm
    box
    breadboard_corner
    breadboard_edge
    breadboard_surface
    cardboard-box
    ceiling-3x3
    cylinder
    colorfilter
    cornell-box-1
    cornell-box-2
    cornell-box-3
    dodecahedron
    door-frame
    door-panel
    duck
    extrusion
    ground-3x3
    laser-plug
    laser-plug-mount
    laser-socket
    level1
    mcube
    mcube-physx
    mirror
    occluder
    octagon
    optic-plug
    optic-socket
    optic-tube-3mm-flat-one-side
    optic-tube-end-5mm
    parking-block
    pbr-cube
    red_led
    round-mirror-mounted
    sensor
    sensor-plate-dark
    sensor-plate-light
    sphere
    sponza
    sponza2
    spotlight-01
    station-segment
    wall-4-corner
    wall-4x3
    wall-4x3-door
)

add_custom_target(models)

# Update the physics collision cache for each model if a physics.json is present.
foreach(_model ${GLTF_MODELS})
    set(physics_path "${PROJECT_SOURCE_DIR}/assets/models/${_model}/${_model}.physics.json")
    if(NOT EXISTS "${physics_path}")
        set(physics_path "${PROJECT_SOURCE_DIR}/assets/models/${_model}/physics.json")
        if(NOT EXISTS "${physics_path}")
            set(physics_path "${PROJECT_SOURCE_DIR}/assets/models/${_model}.physics.json")
            if(NOT EXISTS "${physics_path}")
                set(physics_path "")
            endif()
        endif()
    endif()

    set(model_path "${PROJECT_SOURCE_DIR}/assets/models/${_model}/${_model}.glb")
    if(NOT EXISTS "${model_path}")
        set(model_path "${PROJECT_SOURCE_DIR}/assets/models/${_model}.glb")
        if(NOT EXISTS "${model_path}")
            set(model_path "")
        endif()
    endif()

    add_custom_command(
        COMMAND
            hull_compiler --assets ../assets/ ${_model}
        WORKING_DIRECTORY
            ${PROJECT_SOURCE_DIR}/bin
        OUTPUT
            "${PROJECT_SOURCE_DIR}/assets/cache/collision/${_model}"
        DEPENDS
            hull_compiler
            ${model_path}
            ${physics_path}
    )

    add_custom_target(${_model}-physics DEPENDS "${PROJECT_SOURCE_DIR}/assets/cache/collision/${_model}")
    add_dependencies(models ${_model}-physics)
endforeach()

# Make the project exe depend on having up to date models
add_dependencies(${PROJECT_COMMON_EXE} models)
