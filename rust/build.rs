/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

use std::fs::File;
use std::io::prelude::*;

fn main() {
    let mut build = cxx_build::bridge("src/lib.rs");  // returns a cc::Build
    build.warnings_into_errors(true);
    build.flag_if_supported("-std=c++17");
    build.flag_if_supported("/std:c++17");
    build.define("RUST_CXX", "1");

    let mut includes_file = File::open("../build/rust/rust_includes.list").expect("Failed to open file: rust_includes.list");
    let mut include_list = String::new();
    includes_file.read_to_string(&mut include_list).expect("Failed to read file: rust_includes.list");

    for path in include_list.split(";") {
        if !path.is_empty() {
            build.include(path);
        }
    }
    build.compile("sp-rust");

    // Add source files here and in CMakeLists.txt
    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=src/wasmer_vm.rs");
}
