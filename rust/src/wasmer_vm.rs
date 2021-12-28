use wasmer::{Store, Module, Instance, NativeFunc, imports};
use std::path::Path;

pub fn run_wasm() -> anyhow::Result<i32> {
    let store = Store::default();
    let module = Module::from_file(&store, Path::new("scripts/add.wasm"))?;
    println!("Loaded wasm module: {:?}", module);

    let import_object = imports!{};
    let instance = Instance::new(&module, &import_object)?;
    println!("Loaded wasm instance: {:?}", instance);

    let add: NativeFunc<(i32, i32), i32> = instance.exports.get_native_function("add")?;
    let result = add.call(5, 37)?;
    assert_eq!(result, 42);

    Ok(result)
}
