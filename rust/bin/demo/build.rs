use std::{env, error::Error};

fn main() -> Result<(), Box<dyn Error>> {
    let current_dir = env::current_dir()?;

    println!("cargo:warning={}", current_dir.display());

    println!(
        "cargo:rustc-link-search={}/../../../bin",
        current_dir.display()
    );

    Ok(()) // FIXME
}
