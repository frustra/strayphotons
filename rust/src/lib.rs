/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#[derive(Debug)]
pub struct Context {
    data: String,
}
 
fn new_wasm_context(name: &str) -> Box<Context> {
    println!("Rust callback! name={}", name);
    return Box::new(Context { data: name.to_string() });
}
 
fn wasm_run_on_tick(context: Box<Context>) {
    println!("Rust onTick callback! context.data={}", context.data);
}
 
fn wasm_run_on_physics_update(context: Box<Context>) {
    println!("Rust onPhysicsUpdate callback! context.data={}", context.data);
}
 
fn wasm_run_prefab(context: Box<Context>) {
    println!("Rust prefab callback! context.data={}", context.data);
}

#[cxx::bridge(namespace = "sp::rs")]
mod ffi_rust {
    extern "Rust" {
        fn print_hello();

        type Context;
        fn new_wasm_context(name: &str) -> Box<Context>;
        fn wasm_run_on_tick(context: Box<Context>);
        fn wasm_run_on_physics_update(context: Box<Context>);
        fn wasm_run_prefab(context: Box<Context>);
    }

    unsafe extern "C++" {
        include!("rust/WasmScripts.hh");

        // fn RegisterPrefabScript(name: String);
        fn RegisterOnTickScript(name: String);
        // fn RegisterOnPhysicsUpdateScript(name: String);
    }
}

mod wasmer_vm;

fn print_hello() {
    println!("hello world!");

    let result = wasmer_vm::run_wasm();
    if result.is_err() {
        println!("run_wasm() failed! {}", result.err().unwrap());
    }
    
    // Loop over *.wasm files
    for entry in std::fs::read_dir("scripts/").unwrap() {
        let entry = entry.unwrap();
        if entry.path().extension().and_then(std::ffi::OsStr::to_str) == Some("wasm") {
            // let context = Box::new(Context { data: entry.path().to_str().unwrap().to_string() });
            let name = entry.file_name().to_str().unwrap().to_string();
            ffi_rust::RegisterOnTickScript(name);
        }
    }
}
