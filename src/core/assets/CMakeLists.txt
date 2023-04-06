
target_sources(${PROJECT_CORE_LIB} PRIVATE
    Asset.cc
    AssetManager.cc
    ConsoleScript.cc
    Gltf.cc
    Image.cc
    PhysicsInfo.cc
)

target_precompile_headers(${PROJECT_CORE_LIB} PUBLIC
    Async.hh
)
