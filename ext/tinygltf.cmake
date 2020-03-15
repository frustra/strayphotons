set(TINYGLTF_BUILD_EXAMPLES OFF CACHE BOOL "Disable TinyGLTF building examples" FORCE)
add_subdirectory(tinygltf)

add_library(tinygltf INTERFACE)

target_include_directories(
    tinygltf SYSTEM
    INTERFACE
        ./
        ./tinygltf
)