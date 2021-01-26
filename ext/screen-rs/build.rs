fn main() {
    let _build = cxx_build::bridge("src/lib.rs")  // returns a cc::Build
        .file("src/Api.cc")
        .flag_if_supported("-std=c++11")
        .compile("screen-rs");

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=src/Api.cc");
    println!("cargo:rerun-if-changed=include/Api.hh");
}