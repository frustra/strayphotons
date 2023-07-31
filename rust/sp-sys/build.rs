use cmake::Config;
use std::env;
use std::error::Error;
use std::path::PathBuf;

#[cfg(feature = "debug")]
macro_rules! debug {
    ($($tokens: tt)*) => {
        println!("cargo:warning={}", format!($($tokens)*))
    }
}

fn append_to_path(p: PathBuf, s: &str) -> PathBuf {
    let mut p = p.into_os_string();
    p.push(s);
    p.into()
}

fn main() -> Result<(), Box<dyn Error>> {
    let current_dir = env::current_dir()?;
    let build_dir = append_to_path(current_dir, "/../../build");
    let sp = Config::new("../../")
        .out_dir(build_dir)
        .define("CMAKE_C_COMPILER", "clang")
        .define("CMAKE_CXX_COMPILER", "clang++")
        .define("NO_CXX", "1")
        .generator("Ninja")
        .static_crt(true)
        .uses_cxx11()
        .always_configure(false)
        .build_target("sp")
        .build();

    println!("cargo:rustc-link-search=native={}/src", sp.display());
    println!("cargo:rustc-link-lib=static=sp");

    Ok(())
}
