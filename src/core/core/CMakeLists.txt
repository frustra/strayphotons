
target_sources(${PROJECT_CORE_LIB} PRIVATE
    Common.cc
    Console.cc
    LockFreeMutex.cc
    RegisteredThread.cc
)

target_precompile_headers(${PROJECT_CORE_LIB} PUBLIC
    CFunc.hh
    Common.hh
    CVar.hh
    Logging.hh
)
