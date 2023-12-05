/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

use std::env;
use std::error::Error;

fn main() -> Result<(), Box<dyn Error>> {
    let mut bridges = vec![];
    #[cfg(feature = "api")]
    bridges.push("src/api.rs");
    #[cfg(feature = "wasm")]
    bridges.push("src/wasm.rs");
    #[cfg(feature = "winit")]
    bridges.push("src/winit.rs");

    let mut build = cxx_build::bridges(bridges); // returns a cc::Build

    #[cfg(feature = "api")]
    build.file("src/api.cc");

    build.cpp(true).std("c++20")
        .flag_if_supported("/EHsc")
        .static_crt(true);

    let build_type = env!("CMAKE_BUILD_TYPE", "$CMAKE_BUILD_TYPE env variable not set");
    if build_type == "Debug" {
        println!("cargo:warning=Building in debug mode");
        build.debug(true);
    }

    let include_list = env!("RUST_INCLUDES", "$RUST_INCLUDES env variable not set");
    for path in include_list.split(";") {
        if !path.is_empty() {
            build.include(path);
        }
    }
    build.compile("sp-rs");

    let build_dir = env!("CARGO_TARGET_DIR", "$CARGO_TARGET_DIR env variable not set");
    println!("cargo:warning=Building in directory: {}", build_dir);

    // Add source files here and in CMakeLists.txt
    println!("cargo:rerun-if-changed=src/api.cc");
    println!("cargo:rerun-if-changed=include/api.hh");
    println!("cargo:rerun-if-changed=src/api.rs");
    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=src/wasm.rs");
    println!("cargo:rerun-if-changed=src/winit.rs");
    Ok(())
}
