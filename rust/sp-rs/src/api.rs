#[cxx::bridge(namespace = "sp::api")]
mod ctx {
    unsafe extern "C++" {
        include!("api.hh");

        type Api;

        fn new_api() -> UniquePtr<Api>;
    }
}
