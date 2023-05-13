set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(OUTPUT_DIR ${PROJECT_ROOT_DIR}/assets/shaders)
set(SHADER_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)

file(GLOB_RECURSE SHADERS
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

file(GLOB_RECURSE SHADERS_LIB ${SHADER_DIR}/*.glsl)

find_package(Vulkan REQUIRED)

foreach(SHADER IN LISTS SHADERS)
    string(REPLACE ${SHADER_DIR} "" SHADER_FILE ${SHADER})
    string(REPLACE "vulkan/" "" OUTPUT_NAME ${SHADER_FILE})
    add_custom_command(
        OUTPUT ${OUTPUT_DIR}/${OUTPUT_NAME}.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} -Werror ${SHADER} -o ${OUTPUT_DIR}/${OUTPUT_NAME}.spv
        DEPENDS ${SHADER} ${SHADERS_LIB}
        COMMENT "Compiling shaders${SHADER_FILE}"
    )
    list(APPEND SPV_SHADERS ${OUTPUT_DIR}/${OUTPUT_NAME}.spv)
endForeach()

set(SHADER_FILES ${SHADERS} PARENT_SCOPE)

add_custom_target(shaders ALL DEPENDS ${SPV_SHADERS})
