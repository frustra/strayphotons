add_library(tinygltfloader INTERFACE)

target_include_directories(
    tinygltfloader SYSTEM
    INTERFACE
        ./
        ./tinygltfloader
)