add_library(${PROJECT_CORE_LIB} STATIC)

target_link_libraries(${PROJECT_CORE_LIB} PUBLIC
    glm
    microtar
    murmurhash
    picojson
    robin_hood
    Tecs
    tinygltf
)

if (NOT WIN32)
    target_link_libraries(${PROJECT_CORE_LIB} PRIVATE
        linenoise
    )
endif()

target_include_directories(${PROJECT_CORE_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(assets)
add_subdirectory(core)
add_subdirectory(ecs)
