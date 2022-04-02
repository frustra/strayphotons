target_sources(${PROJECT_CORE_LIB} PRIVATE
    Console.cc
    ConsoleBindingManager.cc
    ConsoleCoreCommands.cc
)

target_precompile_headers(${PROJECT_CORE_LIB} PUBLIC
    CFunc.hh
    CVar.hh
)
