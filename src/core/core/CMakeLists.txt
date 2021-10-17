
target_sources(${PROJECT_CORE_LIB} PRIVATE
    Common.cc
    LockFreeMutex.cc
    RegisteredThread.cc
)

target_precompile_headers(${PROJECT_CORE_LIB} PUBLIC
    Common.hh
    Logging.hh
)

add_subdirectory(console)
add_subdirectory(input)
