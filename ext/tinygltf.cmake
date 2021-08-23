set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "Disable TinyGLTF building examples" FORCE)
add_subdirectory(tinygltf)

add_library(tinygltf INTERFACE)

target_include_directories(
    tinygltf SYSTEM
    INTERFACE
        ./
        ./tinygltf
)
