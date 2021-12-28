use wasmer::{Store, Module, Instance, Function, NativeFunc, imports};
use std::path::Path;

pub fn run_wasm() -> anyhow::Result<i32> {
    let store = Store::default();
    let module = Module::from_file(&store, Path::new("scripts/add.wasm"))?;
    println!("Loaded wasm module");
    
    let import_object = imports! {
        "env" => {
            "hello" => Function::new_native(&store, || println!("Hello from WASM!")),
        }
    };
    let instance = Instance::new(&module, &import_object)?;
    println!("Loaded wasm instance");

    let add: NativeFunc<(i32, i32), i32> = instance.exports.get_native_function("add")?;
    let result = add.call(5, 37)?;
    assert_eq!(result, 42);

    Ok(result)
}
