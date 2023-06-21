/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

use std::path::Path;

use wasmer_vm::WasmContext;

pub struct Context {
    name: String,
    wasm: Option<WasmContext>,
}
 
fn new_wasm_context(name: &str) -> Box<Context> {
    println!("Rust callback! name={}", name);
    let result = wasmer_vm::new_instance(&Path::new("scripts/").join(name));
    if result.is_err() {
        println!("new_instance() failed! {}", result.err().unwrap());
        return Box::new(Context {
            name: name.to_string(),
            wasm: None,
        });
    } else {
        return Box::new(Context {
            name: name.to_string(),
            wasm: result.ok(),
        });
    }
}
 
fn wasm_run_on_tick(mut context: Box<Context>, ent: u64) {
    println!("Rust onTick callback! context.name={}", context.name);
    if context.wasm.is_some() {
        let result = wasmer_vm::call_on_tick(context.wasm.as_mut().unwrap(), ent);
        if result.is_err() {
            println!("call_on_tick() failed! {}", result.err().unwrap());
        }
    } else {
        println!("wasm_run_on_tick() null instance! {}", context.name);
    }
}
 
fn wasm_run_on_physics_update(context: Box<Context>) {
    println!("Rust onPhysicsUpdate callback! context.name={} instance: {}", context.name, context.wasm.map_or("invalid", |_| "valid"));
}
 
fn wasm_run_prefab(context: Box<Context>) {
    println!("Rust prefab callback! context.name={} instance: {}", context.name, context.wasm.map_or("invalid", |_| "valid"));
}

#[cxx::bridge(namespace = "sp::rs")]
mod ffi_rust {
    extern "Rust" {
        fn print_hello();

        type Context;
        fn new_wasm_context(name: &str) -> Box<Context>;
        fn wasm_run_on_tick(mut context: Box<Context>, ent: u64);
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
