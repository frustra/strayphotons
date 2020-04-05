
target_sources(
    ${PROJECT_LIB}
    PRIVATE
        Asset.cc
        AssetHelpers.cc
        AssetManager.cc
        Model.cc
        Scene.cc
)

add_subdirectory(valvepak)
