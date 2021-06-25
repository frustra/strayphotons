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

if(WIN32)
    # Disable warnings for warning C4506: no definition for inline function 'bool ecs::Component<...>::Load()'
    target_compile_options(${PROJECT_CORE_LIB} PUBLIC
        /wd4506
    )
else()
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
