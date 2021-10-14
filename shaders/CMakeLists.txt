## TODO: move shader install() into this folder

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vulkan) # TODO: delete special vulkan subfolder
set(SHADER_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)

file(GLOB SHADERS
    ${SHADER_DIR}/*.vert
    ${SHADER_DIR}/*.frag
    ${SHADER_DIR}/*.comp
    ${SHADER_DIR}/*.geom
    ${SHADER_DIR}/*.tesc
    ${SHADER_DIR}/*.tese
    ${SHADER_DIR}/*.mesh
    ${SHADER_DIR}/*.task
    ${SHADER_DIR}/*.rgen
    ${SHADER_DIR}/*.rchit
    ${SHADER_DIR}/*.rmiss
)

file(GLOB SHADERS_LIB ${SHADER_LIB_DIR}/*.glsl ${SHADER_DIR}/lib/*.glsl)

find_package(Vulkan REQUIRED)

foreach(SHADER IN LISTS SHADERS)
    get_filename_component(FILENAME ${SHADER} NAME)
    add_custom_command(
        OUTPUT ${SHADER_DIR}/bin/${FILENAME}.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} -Werror ${SHADER} -o ${SHADER_DIR}/bin/${FILENAME}.spv
        DEPENDS ${SHADER} ${SHADERS_LIB}
        COMMENT "Compiling ${FILENAME}"
    )
    list(APPEND SPV_SHADERS ${SHADER_DIR}/bin/${FILENAME}.spv)
endForeach()

add_custom_target(shaders ALL DEPENDS ${SPV_SHADERS})
