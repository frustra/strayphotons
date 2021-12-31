#[cxx::bridge(namespace = "ecs")]
mod ffi_ecs {
    extern "C++" {
        include!(<ecs/Ecs.hh>);
    }
}

#[repr(C, packed(4))]
#[derive(Copy, Clone)]
struct GlmVec3 {
    data: [f32; 3],
}

#[repr(C, packed(4))]
#[derive(Copy, Clone)]
struct GlmMat4x3 {
    data: [GlmVec3; 4],
}

#[repr(C, packed(4))]
#[derive(Copy, Clone, Debug)]
struct Transform {
    parent: usize,
    transform: GlmMat4x3,
    change_count: u32,
}

unsafe impl wasmer::ValueType for GlmVec3 {}
unsafe impl wasmer::ValueType for GlmMat4x3 {}
unsafe impl wasmer::ValueType for Transform {}

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

#[allow(unused)]
extern "C" {
    fn transform_identity(out: *mut Transform);
    fn transform_from_pos(pos: *const GlmVec3, out: *mut Transform);
    fn transform_get_position(t: *const Transform, out: *mut GlmVec3);
    fn transform_set_position(t: *const Transform, pos: *const GlmVec3);
}

#[cxx::bridge(namespace = "sp::rust")]
mod ffi_rust {
    extern "Rust" {
        fn print_hello();
    }
}

use wasmer::{Store, Module, Instance, Function, Extern, WasmPtr, NativeFunc, WasmerEnv, imports};
use std::path::Path;
use std::sync::{Arc, Mutex};
use std::mem::MaybeUninit;

fn hello_transform(env: &Env, transform: WasmPtr<Transform>) {
    let instance_lock = env.instance.lock().unwrap();
    let instance = instance_lock.as_ref().unwrap();
    let memory = instance.exports.get_memory("memory").unwrap();
    println!("Hello from WASM! {:?}", transform.deref(memory).unwrap());
}

fn transform_identity_wasm(env: &Env, out: WasmPtr<Transform>) {
    let instance_lock = env.instance.lock().unwrap();
    let instance = instance_lock.as_ref().unwrap();
    let memory = instance.exports.get_memory("memory").unwrap();

    unsafe {
        let mut identity = MaybeUninit::<Transform>::uninit();
        transform_identity(identity.as_mut_ptr());
        println!("transform_identity_wasm {:?}", identity.assume_init());
        out.deref(memory).unwrap().set(identity.assume_init());
    }
}

fn transform_from_pos_wasm(env: &Env, pos: WasmPtr<GlmVec3>, out: WasmPtr<Transform>) {
    let instance_lock = env.instance.lock().unwrap();
    let instance = instance_lock.as_ref().unwrap();
    let memory = instance.exports.get_memory("memory").unwrap();

    let position = pos.deref(memory).unwrap();

    unsafe {
        let mut transform = MaybeUninit::<Transform>::uninit();
        transform_from_pos(&position.get(), transform.as_mut_ptr());
        println!("transform_from_pos_wasm {:?}", transform.assume_init());
        out.deref(memory).unwrap().set(transform.assume_init());
    }
}

fn transform_get_position_wasm(env: &Env, t: WasmPtr<Transform>, out: WasmPtr<GlmVec3>) {
    let instance_lock = env.instance.lock().unwrap();
    let instance = instance_lock.as_ref().unwrap();
    let memory = instance.exports.get_memory("memory").unwrap();

    let transform = t.deref(memory).unwrap();

    unsafe {
        let mut position = MaybeUninit::<GlmVec3>::uninit();
        transform_get_position(&transform.get(), position.as_mut_ptr());
        println!("transform_get_position_wasm {:?}", position.assume_init());
        out.deref(memory).unwrap().set(position.assume_init());
    }
}

#[derive(WasmerEnv, Clone)]
struct Env {
    instance: Arc<Mutex<Option<Instance>>>,
}

fn run_wasm() -> anyhow::Result<()> {
    let store = Store::default();
    let module = Module::from_file(&store, Path::new("scripts/script.wasm"))?;
    println!("Loaded wasm module");
    
    let instance_arc = Arc::new(Mutex::new(None::<Instance>));
    let import_object = imports!{
        "env" => {
            "hello" => Function::new_native_with_env(&store, Env { instance: instance_arc.clone() }, hello_transform),
            "transform_identity" => Function::new_native_with_env(&store, Env { instance: instance_arc.clone() }, transform_identity_wasm),
            "transform_from_pos" => Function::new_native_with_env(&store, Env { instance: instance_arc.clone() }, transform_from_pos_wasm),
            "transform_get_position" => Function::new_native_with_env(&store, Env { instance: instance_arc.clone() }, transform_get_position_wasm),
        }
    };
    let instance = Instance::new(&module, &import_object)?;
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

    let mut a = Transform{parent: !0, transform: GlmMat4x3{data: [GlmVec3{data: [0f32; 3]}; 4]}, change_count: 1};
    let mut b = a.clone();
    unsafe {
        transform_identity(&mut a);
        transform_from_pos(&GlmVec3{data: [1.0, 2.0, 3.0]}, &mut b);
        let mut pos_a = MaybeUninit::<GlmVec3>::uninit();
        let mut pos_b = MaybeUninit::<GlmVec3>::uninit();
        transform_get_position(&a, pos_a.as_mut_ptr());
        transform_get_position(&b, pos_b.as_mut_ptr());
        println!("A: {:?}, B: {:?}", pos_a.assume_init(), pos_b.assume_init());
    }

    let add: NativeFunc<(WasmPtr<Transform>, WasmPtr<Transform>, WasmPtr<Transform>)> = instance.exports.get_native_function("add")?;
    
    {
        let mut instance_lock = instance_arc.lock().unwrap();
        instance_lock.replace(instance);
    }

    let memory_offset: u32;
    let memory_stride = std::mem::size_of::<Transform>() as u32;
    {
        let instance_lock = instance_arc.lock().unwrap();
        let instance = instance_lock.as_ref().unwrap();
        let memory = instance.exports.get_memory("memory").unwrap();
        
        memory_offset = memory.grow(1)?.bytes().0 as u32;
        unsafe {
            let host_memory = memory.data_ptr().offset(memory_offset as isize);
            let dst: *mut Transform = std::mem::transmute(host_memory);
            std::ptr::write(dst, a);
            std::ptr::write(dst.add(1), b);
        }
    }

    add.call(
        WasmPtr::new(memory_offset + 2 * memory_stride),
        WasmPtr::new(memory_offset),
        WasmPtr::new(memory_offset + memory_stride))?;
    {
        let instance_lock = instance_arc.lock().unwrap();
        let instance = instance_lock.as_ref().unwrap();
        let memory = instance.exports.get_memory("memory").unwrap();
        
        println!("Result: {:?}", WasmPtr::<Transform>::new(memory_offset + 2 * memory_stride).deref(memory).unwrap());
    }
    Ok(())
}

pub fn print_hello() {
    println!("hello world!");

    let result = run_wasm();
    if result.is_err() {
        println!("run_wasm() failed! {}", result.err().unwrap());
    }
}
