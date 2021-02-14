fn main() {
    let _build = cxx_build::bridge("src/lib.rs")  // returns a cc::Build
        .flag_if_supported("-std=c++17")
        .compile("sp-rust");

    println!("cargo:rerun-if-changed=src/lib.rs");
}
