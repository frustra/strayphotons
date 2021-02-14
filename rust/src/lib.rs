
#[cxx::bridge(namespace = "sp::rust")]
mod ffi {
    extern "Rust" {
        fn print_hello();
        fn http_get(url: String) -> String;
    }
}

pub fn print_hello() {
    println!("hello world!");
}

pub fn http_get(url: String) -> String {
    let res = reqwest::blocking::get(url::Url::parse(&url).ok().unwrap());
    if res.is_ok() {
        let content = res.ok().unwrap().text();
        if content.is_ok() {
            return content.ok().unwrap();
        } else {
            return "no content".to_string();
        }
    } else {
        return "error".to_string();
    }
}
