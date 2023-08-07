/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#[cxx::bridge(namespace = "sp::rust")]
mod ffi_rust {
    extern "Rust" {
        fn print_hello();
    }
}

mod wasmer_vm;

fn print_hello() {
    println!("hello world!");

    let result = wasmer_vm::run_wasm();
    if result.is_err() {
        println!("run_wasm() failed! {}", result.err().unwrap());
    }
}
