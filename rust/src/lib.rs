
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

pub fn print_hello() {
    println!("hello world!");
}
