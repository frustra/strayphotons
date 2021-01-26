fn main() {
    let _build = cxx_build::bridge("src/lib.rs")  // returns a cc::Build
        .file("src/Api.cc")
        .flag_if_supported("-std=c++11");

    println!("cargo:rerun-if-changed=src/lib.rs");
}