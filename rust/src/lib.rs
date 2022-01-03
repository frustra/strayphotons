use wasmer::{Store, Module, Instance, Function, Extern, WasmPtr, NativeFunc, WasmerEnv, imports};
use std::path::Path;
use std::sync::{Arc, Mutex};
use std::mem::MaybeUninit;
use static_assertions::assert_eq_size;

#[cxx::bridge(namespace = "ecs")]
mod ffi_ecs {
    extern "C++" {
        include!(<ecs/Ecs.hh>);
    }
}

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
    transform: GlmMat4x3,
    parent: u64,
    change_count: u32,
}
assert_eq_size!(Transform, [u8; 64]);

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

#[cxx::bridge(namespace = "sp::rust")]
mod ffi_rust {
    extern "Rust" {
        fn print_hello();
    }
}

#[derive(WasmerEnv, Clone)]
pub struct Env {
    instance: Arc<Mutex<Option<Instance>>>,
}

macro_rules! wasm_to_c_helper {
    ($return_type:ty, $func_name:ident ($($arg:ident : $arg_type:ty),*)) => {
        extern "C" {
            fn $func_name(out: *mut $return_type $(, $arg: *const $arg_type)*);
        }

        mod $func_name {
            use wasmer::WasmPtr;

            #[allow(unused)]
            pub fn call(env: &crate::Env, out: WasmPtr<$return_type> $(, $arg: WasmPtr<$arg_type>)*) {
                let instance_lock = env.instance.lock().unwrap();
                let instance = instance_lock.as_ref().unwrap();
                let memory = instance.exports.get_memory("memory").unwrap();

                let return_value = out.deref(memory).unwrap();
                $(
                    let $arg = $arg.deref(memory).unwrap();
                )*
            
                unsafe {
                    #[allow(deprecated)]
                    crate::$func_name(return_value.get_mut() $(, $arg.get_mut())*);
                    println!("{} {:?}", stringify!($func_name), return_value);
                }
            }
        }
    };
}

fn hello_transform(env: &Env, transform: WasmPtr<Transform>) {
    let instance_lock = env.instance.lock().unwrap();
    let instance = instance_lock.as_ref().unwrap();
    let memory = instance.exports.get_memory("memory").unwrap();
    println!("Hello from WASM! {:?}", transform.deref(memory).unwrap());
}

wasm_to_c_helper!(crate::Transform, transform_identity());
wasm_to_c_helper!(crate::Transform, transform_from_pos(pos: crate::GlmVec3));
wasm_to_c_helper!(crate::GlmVec3, transform_get_position(t: crate::Transform));
wasm_to_c_helper!(crate::Transform, transform_set_position(pos: crate::GlmVec3));


fn run_wasm() -> anyhow::Result<()> {
    let store = Store::default();
    let module = Module::from_file(&store, Path::new("scripts/script.wasm"))?;
    println!("Loaded wasm module");
    
    let instance_arc = Arc::new(Mutex::new(None::<Instance>));
    let import_object = imports!{
        "env" => {
            "hello" => Function::new_native_with_env(&store, Env { instance: instance_arc.clone() }, hello_transform),
            "transform_identity" => Function::new_native_with_env(&store, Env { instance: instance_arc.clone() }, transform_identity::call),
            "transform_from_pos" => Function::new_native_with_env(&store, Env { instance: instance_arc.clone() }, transform_from_pos::call),
            "transform_get_position" => Function::new_native_with_env(&store, Env { instance: instance_arc.clone() }, transform_get_position::call),
            "transform_set_position" => Function::new_native_with_env(&store, Env { instance: instance_arc.clone() }, transform_set_position::call),
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
        transform_from_pos(&mut b, &GlmVec3{data: [1.0, 2.0, 3.0]});
        let mut pos_a = MaybeUninit::<GlmVec3>::uninit();
        let mut pos_b = MaybeUninit::<GlmVec3>::uninit();
        transform_get_position(pos_a.as_mut_ptr(), &a);
        transform_get_position(pos_b.as_mut_ptr(), &b);
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
