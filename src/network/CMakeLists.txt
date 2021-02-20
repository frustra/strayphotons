add_subdirectory(protocol)
add_subdirectory(util)

target_sources(
    ${PROJECT_LIB}
    PRIVATE
    NetworkManager.cc
    ServerHandler.cc
)
