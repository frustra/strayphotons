add_custom_command(
    OUTPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/Protocol.pb.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/Protocol.pb.cc"
    COMMAND protoc Protocol.proto -I. --cpp_out=.
    MAIN_DEPENDENCY ./Protocol.proto
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_custom_target(protocol_target DEPENDS Protocol.pb.h Protocol.pb.cc)
add_dependencies(${PROJECT_LIB} protocol_target)

target_sources(
    ${PROJECT_LIB}
    PRIVATE
        Protocol.pb.h
        Protocol.pb.cc
)
