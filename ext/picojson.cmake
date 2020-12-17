add_library(picojson INTERFACE)

target_include_directories(
    picojson
    INTERFACE
        ./
        ./picojson
)

target_compile_definitions(
    picojson
    INTERFACE 
        PICOJSON_USE_INT64
)
