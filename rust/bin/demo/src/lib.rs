extern crate sp_sys;

use mobile_entry_point::mobile_entry_point;
use sp_sys::StrayPhotons;
use std::error::Error;
use std::env;

#[cfg(target_os = "android")]
fn init_logging() -> Result<(), Box<dyn Error>> {
    android_logger::init_once(
        android_logger::Config::default()
            .with_max_level(log::LevelFilter::Trace)
            .with_tag("demo"),
    );

    Ok(())
}

#[cfg(not(target_os = "android"))]
fn init_logging() -> Result<(), Box<dyn Error>> {
    simple_logger::SimpleLogger::new().init()?;
    Ok(())
}

fn start() -> Result<(), Box<dyn Error>> {
    init_logging()?;
    log::warn!("start");

    let sp = StrayPhotons::new();

    let current_dir = env::current_dir()?;
    log::info!("Rust working directory: {}", current_dir.display());

    log::warn!("init");

    // TODO: API for setting scripts/assets
    unsafe {
        sp.start();
    }

    Ok(())
}

#[mobile_entry_point]
fn main() {
    match start() {
        Ok(_) => {},
        Err(err) => {
            eprintln!("Error: {}", err);
        },
    }
}
