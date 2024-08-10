/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

use std::error::Error;

fn main() -> Result<(), Box<dyn Error>> {
    let mut build = cxx_build::bridges(vec!["src/winit.rs"]); // returns a cc::Build

    build.cpp(true)
        .flag_if_supported("/std:c++20")
        .flag_if_supported("-std:c++20")
        .flag_if_supported("/EHsc")
        .static_crt(true);

    let build_type = option_env!("CMAKE_BUILD_TYPE");
    if build_type == Some("Debug") {
        println!("cargo:warning=Building in debug mode");
        build.debug(true);
    }

    let include_list_opt = option_env!("RUST_INCLUDES");
    if let Some(include_list) = include_list_opt {
        for path in include_list.split(";") {
            if !path.is_empty() {
                build.include(path);
            }
        }
    } else {
        build.include("../../src/common/");
        build.include("../../ext/robin-hood-hashing/src/include/");
    }
    build.compile("sp-rs-winit");

    let build_dir_opt = option_env!("CARGO_TARGET_DIR");
    if let Some(build_dir) = build_dir_opt {
        println!("cargo:warning=Building in directory: {}", build_dir);
    }

    // Add source files here and in CMakeLists.txt
    println!("cargo:rerun-if-changed=src/winit.rs");
    Ok(())
}
