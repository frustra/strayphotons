
add_library(murmurhash STATIC murmurhash/MurmurHash3.cpp)

target_include_directories(
    murmurhash
    PUBLIC
        murmurhash
)