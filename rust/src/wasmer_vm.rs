/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

use wasmer::{Store, Module, Instance, Function, Extern, WasmPtr, FunctionEnvMut, FunctionEnv, imports, Memory, TypedFunction};
use std::path::Path;
use std::mem::MaybeUninit;
use static_assertions::assert_eq_size;

pub type Entity = u64;

#[repr(C, packed(4))]
#[derive(Copy, Clone)]
pub struct GlmVec3 {
    data: [f32; 3],
}

#[repr(C, packed(4))]
#[derive(Copy, Clone)]
pub struct GlmMat4x3 {
    data: [GlmVec3; 4],
}

#[repr(C, packed(8))]
#[derive(Copy, Clone, Debug)]
pub struct Transform {
    offset: GlmMat4x3,
    scale: GlmVec3,
}
// If this changes, make sure it is the same in C++ and WASM
assert_eq_size!(Transform, [u8; 60]);

unsafe impl wasmer::ValueType for GlmVec3 {
    fn zero_padding_bytes(&self, _bytes: &mut [MaybeUninit<u8>]) {}
}
unsafe impl wasmer::ValueType for GlmMat4x3 {
    fn zero_padding_bytes(&self, _bytes: &mut [MaybeUninit<u8>]) {}
}
unsafe impl wasmer::ValueType for Transform {
    fn zero_padding_bytes(&self, _bytes: &mut [MaybeUninit<u8>]) {}
}

impl std::fmt::Debug for GlmVec3 {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{:?}", self.data)
    }
}

impl std::fmt::Debug for GlmMat4x3 {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{:?}", self.data)
    }
}

pub struct Env {
    memory: Option<Memory>,
}

macro_rules! wasm_to_c_helper {
    ($return_type:ty, $func_name:ident ($($arg:ident : $arg_type:ty),*)) => {
        extern "C" {
            fn $func_name(out: *mut $return_type $(, $arg: *const $arg_type)*);
        }

        mod $func_name {
            use wasmer::WasmPtr;
            use crate::wasmer_vm::*;

            #[allow(unused)]
            pub fn call(mut env: FunctionEnvMut<Env>, out: WasmPtr<$return_type> $(, $arg: WasmPtr<$arg_type>)*) {
                let (env_mut, store_mut) = env.data_and_store_mut();
                let memory = env_mut.memory.as_ref().unwrap();
                let memory_view = memory.view(&store_mut);

                unsafe {
                    let mut return_ref = out.access(&memory_view).unwrap();
                    let mut return_ptr: *mut $return_type = std::mem::transmute(return_ref.as_mut());
                    $(
                        let mut arg_ref = $arg.access(&memory_view).unwrap();
                        let mut $arg: *mut $arg_type = std::mem::transmute(arg_ref.as_mut());
                    )*

                    $func_name(return_ptr $(, $arg)*);
                    println!("{} {:?}", stringify!($func_name), return_ref.as_ref());
                }
            }
        }
    };
}

fn print_str(mut env: FunctionEnvMut<Env>, str: WasmPtr<u8>) {
    let (env_mut, store_mut) = env.data_and_store_mut();
    let memory = env_mut.memory.as_ref().unwrap();
    let memory_view = memory.view(&store_mut);
    println!("print_str: {:?}", str.read_utf8_string_with_nul(&memory_view).unwrap_or("invalid ptr".to_string()));
}

fn print_transform(mut env: FunctionEnvMut<Env>, transform_ptr: WasmPtr<Transform>) {
    let (env_mut, store_mut) = env.data_and_store_mut();
    let memory = env_mut.memory.as_ref().unwrap();
    let memory_view = memory.view(&store_mut);
    let transform = &transform_ptr.deref(&memory_view).read().unwrap();
    println!("print_transform: {:?}", transform);
}

wasm_to_c_helper!(Transform, transform_identity());
wasm_to_c_helper!(Transform, transform_from_pos(pos: GlmVec3));
wasm_to_c_helper!(GlmVec3, transform_get_position(t: Transform));
wasm_to_c_helper!(Transform, transform_set_position(pos: GlmVec3));

pub struct WasmContext {
    store: Store,
    instance: Instance,
}

pub fn new_instance(path: &Path) -> anyhow::Result<WasmContext> {
    let mut store = Store::default();
    let module = Module::from_file(&store, path)?;
    println!("Loaded wasm module");
    
    let env = FunctionEnv::new(&mut store, Env {
        memory: None,
    });
    let import_object = imports!{
        "env" => {
            "print_str" => Function::new_typed_with_env(&mut store, &env, print_str),
            "print_transform" => Function::new_typed_with_env(&mut store, &env, print_transform),
            "transform_identity" => Function::new_typed_with_env(&mut store, &env, transform_identity::call),
            "transform_from_pos" => Function::new_typed_with_env(&mut store, &env, transform_from_pos::call),
            "transform_get_position" => Function::new_typed_with_env(&mut store, &env, transform_get_position::call),
            "transform_set_position" => Function::new_typed_with_env(&mut store, &env, transform_set_position::call),
        }
    };
    let instance = Instance::new(&mut store,&module, &import_object)?;
    let mut env_mut = env.into_mut(&mut store);
    let mut data_mut = env_mut.data_mut();
    data_mut.memory = Some(instance.exports.get_memory("memory")?.clone());
    println!("Loaded wasm instance");

    for (name, ext) in instance.exports.iter() {
        match ext {
            Extern::Function(func) => {
                println!("Function: {}: {:?}", name, func);
            },
            Extern::Global(global) => {
                println!("Global: {}: {:?}", name, global);
            },
            _ => ()
        }
    }

    return Ok(WasmContext {
        store: store,
        instance: instance,
    });
}

pub fn call_on_tick(ctx: &mut WasmContext, ent: Entity) -> anyhow::Result<()> {
    let on_tick: TypedFunction<Entity, ()> =ctx.instance.exports.get_typed_function(&mut ctx.store, "onTick")?;

    on_tick.call(&mut ctx.store, ent)?;
    Ok(())
}

unsafe fn any_as_u8_slice<T: Sized>(p: &T) -> &[u8] {
    ::core::slice::from_raw_parts(
        (p as *const T) as *const u8,
        std::mem::size_of::<T>(),
    )
}

pub fn run_wasm() -> anyhow::Result<()> {
    let mut a = Transform{
        offset: GlmMat4x3{data: [GlmVec3{data: [0f32; 3]}; 4]},
        scale: GlmVec3{data: [0f32; 3]},
    };
    let mut b = a.clone();
    unsafe {
        transform_identity(&mut a);
        transform_from_pos(&mut b, &GlmVec3{data: [1.0, 2.0, 3.0]});
        let mut pos_a = MaybeUninit::<GlmVec3>::uninit();
        let mut pos_b = MaybeUninit::<GlmVec3>::uninit();
        transform_get_position(pos_a.as_mut_ptr(), &a);
        transform_get_position(pos_b.as_mut_ptr(), &b);
        println!("A: {:?}, B: {:?}", pos_a.assume_init(), pos_b.assume_init());
    }

    let mut ctx = new_instance(&Path::new("scripts/script.wasm")).unwrap();
    let add: TypedFunction<(WasmPtr<Transform>, WasmPtr<Transform>, WasmPtr<Transform>), ()> =
        ctx.instance.exports.get_typed_function(&mut ctx.store, "add")?;

    let memory_offset: u32;
    let memory_stride = std::mem::size_of::<Transform>() as u32;

    {
        let memory = ctx.instance.exports.get_memory("memory").unwrap();
        
        memory_offset = memory.grow(&mut ctx.store, 1)?.bytes().0 as u32;
        let memory_view = memory.view(&ctx.store);
        unsafe {
            memory_view.write(memory_offset as u64, any_as_u8_slice(&a))?;
            memory_view.write((memory_offset + memory_stride) as u64, any_as_u8_slice(&b))?;
        }
    }

    add.call(
        &mut ctx.store,
        WasmPtr::new(memory_offset + 2 * memory_stride),
        WasmPtr::new(memory_offset),
        WasmPtr::new(memory_offset + memory_stride))?;

    {
        let memory = ctx.instance.exports.get_memory("memory").unwrap();
        let memory_view = memory.view(&ctx.store);

        let result_ptr = WasmPtr::<Transform>::new(memory_offset + 2 * memory_stride);
        let result = &result_ptr.deref(&memory_view).read().unwrap();
        println!("Result: {:?}", result);
    }

    Ok(())
}
