target_sources(${PROJECT_CORE_LIB} PRIVATE
    ConsoleBindingManager.cc
)

target_precompile_headers(${PROJECT_CORE_LIB} PUBLIC
    BindingNames.hh
    KeyCodes.hh
)
