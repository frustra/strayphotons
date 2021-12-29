#[cxx::bridge(namespace = "Tecs")]
mod ffi_Tecs {
    extern "C++" {
        include!(<Tecs.hh>);
    }
}

#[cxx::bridge(namespace = "ecs")]
mod ffi_ecs {
    unsafe extern "C++" {
        include!(<ecs/Ecs.hh>);
    }
}

#[cxx::bridge(namespace = "sp::rust")]
mod ffi_rust {
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
