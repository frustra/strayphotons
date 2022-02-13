
target_sources(${PROJECT_CORE_LIB} PRIVATE
    Common.cc
    DispatchQueue.cc
    LockFreeMutex.cc
    Logging.cc
    RegisteredThread.cc
)

target_precompile_headers(${PROJECT_CORE_LIB} PUBLIC
    Common.hh
    Logging.hh
    Tracing.hh
)
