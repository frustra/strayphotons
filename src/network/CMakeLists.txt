add_subdirectory(protocol)

target_sources(
    ${PROJECT_LIB}
    PRIVATE
        NetworkManager.cc
)
