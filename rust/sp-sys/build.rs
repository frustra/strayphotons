use bindgen::builder;
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
    let bindings = builder()
        .header("../../src/main/strayphotons.h")
        .generate()?;
    bindings.write_to_file("src/game.rs")?;

    let current_dir = env::current_dir()?;
    let build_dir = append_to_path(current_dir, "/../../");
    let mut sp = Config::new("../../");

    //#[cfg(target_os = "android")] // FIXME: check env::var("TARGET")
    sp.define("ANDROID_NDK", env::var("NDK_HOME")?);
    sp.define("CMAKE_ANDROID_API", "24"); // min api version for ndk vulkan support

    let sp = sp
        .out_dir(build_dir)
        .generator("Ninja")
        .pic(true)
        .static_crt(true)
        .uses_cxx11()
        .always_configure(false)
        .build_target("sp")
        .build();

    println!("cargo:rustc-link-search=native={}/build/src", sp.display());
    println!("cargo:rustc-link-lib=sp");

    Ok(())
}
