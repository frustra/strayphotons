fn main() {
    let mut build = cxx_build::bridge("src/lib.rs");  // returns a cc::Build
    build.flag_if_supported("-std=c++17");

    for path in env!("CMAKE_ADDITIONAL_INCLUDES").split(" ") {
        if !path.is_empty() {
            build.include(path);
        }
    }
    build.compile("sp-rust");

    println!("cargo:rerun-if-changed=src/lib.rs");
}
