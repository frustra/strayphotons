fn main() {
    let _build = cxx_build::bridge("src/lib.rs")  // returns a cc::Build
        .flag_if_supported("-std=c++11")
        .include("/home/nauscar/Development/strayphotons/ext/physx/pxshared/include/foundation")
        .include("/home/nauscar/Development/strayphotons/ext/physx/physx/include")
        .include("/home/nauscar/Development/strayphotons/src")
        .include("/home/nauscar/Development/strayphotons/src/graphics/postprocess") // FIXME: Relative paths?
        //.file("Api.cc")
        .compile("screen-rs");

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=src/Api.cc");
    println!("cargo:rerun-if-changed=include/Api.hh");
}