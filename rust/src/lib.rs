
#[cxx::bridge(namespace = "sp::rust")]
mod ffi {
    extern "C++" {
        include!(<Tecs.hh>);
        include!(<ecs/Ecs.hh>);

        // #[cxx_name = "ecs::ECS"]
        // type ECS;
    }

    extern "Rust" {
        fn print_hello();
    }
}

mod wasmer_vm;

pub fn print_hello() {
    println!("hello world!");

    if let Ok(result) = wasmer_vm::run_wasm() {
        println!("add_one(42) = {}", result);
    }
}
