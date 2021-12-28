
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

    let result = wasmer_vm::run_wasm();
    if let Ok(sum) = result {
        println!("add(5, 37) = {}", sum);
    } else {
        println!("add() failed! {}", result.err().unwrap());
    }
}
