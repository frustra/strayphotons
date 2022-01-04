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
