
#[cxx::bridge(namespace = "sp::rust")]
mod ffi {
    extern "Rust" {
        fn print_hello();
    }
}

pub fn print_hello() {
    println!("hello world!");
}
