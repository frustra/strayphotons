fn main() {
    let _build = cxx_build::bridge("src/lib.rs")  // returns a cc::Build
        .flag_if_supported("-std=c++17")
        .include("src")
        .file("src/Api.cc")
        .compile("screen-rs");

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=src/Api.cc");
    println!("cargo:rerun-if-changed=include/Api.hh");
}