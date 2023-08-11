use bindgen::builder;
use cmake::Config;
use std::env;
use std::error::Error;

#[cfg(feature = "debug")]
macro_rules! debug {
    ($($tokens: tt)*) => {
        println!("cargo:warning={}", format!($($tokens)*))
    }
}

fn main() -> Result<(), Box<dyn Error>> {
    let bindings = builder()
        .header("../../include/strayphotons.h")
        .generate()?;
    bindings.write_to_file("src/game.rs")?;

    let current_dir = env::current_dir()?;
    let bin_dir = current_dir.join("../../bin").canonicalize()?;
    let mut sp = Config::new("../../");

    //#[cfg(target_os = "android")] // FIXME: check env::var("TARGET")
    // sp.define("ANDROID_NDK", env::var("NDK_HOME")?);
    // sp.define("CMAKE_ANDROID_API", "24"); // min api version for ndk vulkan support

    let sp = sp
        .generator("Ninja")
        .pic(true)
        .static_crt(true)
        .uses_cxx11()
        .always_configure(true)
        .build_target("sp")
        .build();

    debug!("Rust output dir: {}", sp.display());
    debug!("CMake link dir: {}", bin_dir.display());

    println!("cargo:rustc-link-search=native={}/build/src", sp.display()); // Static library path
    println!("cargo:rustc-link-search=native={}", bin_dir.display()); // Shared library path
    println!("cargo:rustc-link-lib=dylib=sp");

    Ok(())
}
