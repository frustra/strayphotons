set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "Disable TinyGLTF building examples" FORCE)
add_subdirectory(tinygltf)

add_library(tinygltf INTERFACE)

target_compile_definitions(tinygltf INTERFACE TINYGLTF_NO_FS=1)

target_include_directories(tinygltf INTERFACE ./tinygltf)
