/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

use std::env;
use std::error::Error;
use std::fs::File;
use std::io::prelude::*;

fn main() -> Result<(), Box<dyn Error>> {
    /*let current = env::current_dir()?;
    let out = current.join("../../build/rust/sp-rs/cargo/cxxbridge/sp-rs/src/api.hh");
    File::create(out)?.write_all(include_bytes!("include/api.hh"))?;*/

    let mut bridges = vec![];
    #[cfg(feature = "api")]
    bridges.push("src/api.rs");
    #[cfg(feature = "gfx")]
    bridges.push("src/gfx.rs");
    #[cfg(feature = "wasm")]
    bridges.push("src/wasm.rs");

    let mut build = cxx_build::bridges(bridges); // returns a cc::Build

    #[cfg(feature = "api")]
    build.file("src/api.cc");

    build.flag_if_supported("-std=c++20").static_crt(true);

    let mut includes_file = File::open("../../build/rust/sp-rs/rust_includes.list")
        .expect("Failed to open file: rust_includes.list");
    let mut include_list = String::new();
    includes_file
        .read_to_string(&mut include_list)
        .expect("Failed to read file: rust_includes.list");

    for path in include_list.split(";") {
        if !path.is_empty() {
            build.include(path);
        }
    }
    build.compile("sp-rs");

    // Add source files here and in CMakeLists.txt
    println!("cargo:rerun-if-changed=src/api.rs");
    println!("cargo:rerun-if-changed=src/api.cc");
    println!("cargo:rerun-if-changed=src/api.hh");
    println!("cargo:rerun-if-changed=src/gfx.rs");
    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=src/wasm.rs");
    Ok(())
}
