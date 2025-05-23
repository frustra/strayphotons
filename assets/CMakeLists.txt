#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

# Make asset-list.txt a build dependency
configure_file(asset-list.txt asset-list.txt COPYONLY)

message(STATUS "Checking for missing assets...")

# Read the asset-list.txt file and download each of the paths into the assets folder
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/asset-list.txt asset_list_contents)
string(REPLACE "\n" ";" asset_list "${asset_list_contents}")
foreach(asset_line ${asset_list})
    string(REGEX REPLACE "[ \t\r\n]+" ";" asset_split ${asset_line})

    # If the line has an md5 hash, validate it, otherwise only check if it exists.
    list(LENGTH asset_split asset_split_len)
    if(asset_split_len GREATER 2)
        list(GET asset_split 0 asset_md5)
        list(GET asset_split 1 asset_url)
        list(GET asset_split 2 asset_path)

        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${asset_path}")
            message(STATUS "Downloading: ${asset_path} from ${asset_url}")
        endif()

        file(DOWNLOAD
            "${asset_url}?request_md5=${asset_md5}"
            "${CMAKE_CURRENT_SOURCE_DIR}/${asset_path}"
            EXPECTED_HASH "MD5=${asset_md5}"
        )
    endif()
endforeach(asset_line)

add_subdirectory(models)

if(SP_PACKAGE_RELEASE)
    # When adding new asset files, CMake will need to be re-run due to GLOB
    set(_asset_filename assets.spdata)
    file(GLOB_RECURSE _cache_assets RELATIVE "${CMAKE_CURRENT_LIST_DIR}" CONFIGURE_DEPENDS "cache/collision/*")
    file(GLOB_RECURSE _cache_assets_full "cache/collision/*")
    file(GLOB_RECURSE _glb_assets RELATIVE "${CMAKE_CURRENT_LIST_DIR}" CONFIGURE_DEPENDS "models/*.glb")
    file(GLOB_RECURSE _glb_assets_full "models/*.glb")
    file(GLOB_RECURSE _audio_assets RELATIVE "${CMAKE_CURRENT_LIST_DIR}" CONFIGURE_DEPENDS "audio/*.ogg")
    file(GLOB_RECURSE _audio_assets_full "audio/*.ogg")
    file(GLOB_RECURSE _texture_assets RELATIVE "${CMAKE_CURRENT_LIST_DIR}" CONFIGURE_DEPENDS "textures/*.tga" "textures/*.jpg" "textures/*.png")
    file(GLOB_RECURSE _texture_assets_full "textures/*.tga" "textures/*.jpg" "textures/*.png")
    file(GLOB_RECURSE _scene_assets RELATIVE "${CMAKE_CURRENT_LIST_DIR}" CONFIGURE_DEPENDS "scenes/*.json")
    file(GLOB_RECURSE _scene_assets_full "scenes/*.json")
    file(GLOB_RECURSE _logo_assets RELATIVE "${CMAKE_CURRENT_LIST_DIR}" CONFIGURE_DEPENDS "logos/*.png")
    file(GLOB_RECURSE _logo_assets_full "logos/*.png")
    file(GLOB_RECURSE _font_assets RELATIVE "${CMAKE_CURRENT_LIST_DIR}" CONFIGURE_DEPENDS "fonts/*.ttf")
    file(GLOB_RECURSE _font_assets_full "fonts/*.ttf")
    file(GLOB_RECURSE _physics_json_assets RELATIVE "${CMAKE_CURRENT_LIST_DIR}" CONFIGURE_DEPENDS "models/*physics.json")
    file(GLOB_RECURSE _physics_json_assets_full "models/*physics.json")

    foreach(SHADER IN LISTS SHADER_FILES)
        string(REPLACE ${PROJECT_ROOT_DIR}/shaders/ "" SHADER_FILE ${SHADER})
        string(REPLACE "vulkan/" "" OUTPUT_NAME ${SHADER_FILE})
        list(APPEND _shaders_assets shaders/${OUTPUT_NAME}.spv)
        list(APPEND _shaders_assets_full ${PROJECT_ROOT_DIR}/assets/shaders/${OUTPUT_NAME}.spv)
    endForeach()

    add_custom_command(
        COMMAND
            ${CMAKE_COMMAND} -E tar c ${PROJECT_OUTPUT_DIR}/${_asset_filename} --
                default_input_bindings.json
                ${_cache_assets}
                ${_glb_assets}
                ${_font_assets}
                ${_logo_assets}
                ${_audio_assets}
                ${_scene_assets}
                ${_texture_assets}
                ${_shaders_assets}
                ${_physics_json_assets}
        OUTPUT
            ${PROJECT_OUTPUT_DIR}/${_asset_filename}
        DEPENDS
            default_input_bindings.json
            ${_cache_assets_full}
            ${_glb_assets_full}
            ${_font_assets_full}
            ${_logo_assets_full}
            ${_audio_assets_full}
            ${_scene_assets_full}
            ${_texture_assets_full}
            ${_shaders_assets_full}
            ${_physics_json_assets_full}
        WORKING_DIRECTORY
            ${CMAKE_CURRENT_LIST_DIR}
    )

    add_custom_target(assets_tar ALL DEPENDS ${PROJECT_OUTPUT_DIR}/${_asset_filename})

    # Make the TAR generation depend on having up-to-date models
    add_dependencies(assets_tar models)
endif()
