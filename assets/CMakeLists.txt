# Make asset-list.txt a build dependency
configure_file(asset-list.txt asset-list.txt COPYONLY)

message(STATUS "Downloading missing assets...")
set(frustra_s3_auth "github-repo:M9vS5IjJ&6%FHxCV")

# Read the asset-list.txt file and download each of the paths into the assets folder
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/asset-list.txt asset_list_contents)
string(REPLACE "\n" ";" asset_list "${asset_list_contents}")
foreach(asset_line ${asset_list})
    string(REGEX REPLACE "[ \t\r\n]+" ";" asset_split ${asset_line})

    # If the line has an md5 hash, validate it, otherwise only check if it exists.
    list(LENGTH asset_split asset_split_len)
    if(asset_split_len GREATER 1)
        list(GET asset_split 0 asset_md5)
        list(GET asset_split 1 asset_path)
    
        file(DOWNLOAD
            "https://dd10eb56pu043.cloudfront.net/assets/${asset_path}"
            "${CMAKE_CURRENT_SOURCE_DIR}/${asset_path}"
            USERPWD "${frustra_s3_auth}"
            EXPECTED_HASH "MD5=${asset_md5}"
        )
    else()
        list(GET asset_split 0 asset_path)

        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${asset_path}")
            file(DOWNLOAD
                "https://dd10eb56pu043.cloudfront.net/assets/${asset_path}"
                "${CMAKE_CURRENT_SOURCE_DIR}/${asset_path}"
                USERPWD "${frustra_s3_auth}"
                STATUS download_result
            )
            list(GET download_result 0 result_code)

            if(${result_code})
                # If the HTTP request errors, CMake still leaves an empty file around.
                if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${asset_path}")
                    file(REMOVE "${CMAKE_CURRENT_SOURCE_DIR}/${asset_path}")
                endif()

                message(FATAL_ERROR "Failed to download asset file: ${CMAKE_CURRENT_SOURCE_DIR}/${asset_path}")
            endif()
        endif()
    endif()
endforeach(asset_line)

add_subdirectory(models)
add_subdirectory(scenes)