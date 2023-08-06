extern crate sp_sys;

use mobile_entry_point::mobile_entry_point;
use sp_sys::StrayPhotons;
use std::error::Error;

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

    log::warn!("init");

    unsafe {
        sp.start()?;
    }
    // TODO: API for setting scripts/assets

    Ok(())
}

#[mobile_entry_point]
fn main() {
    match start() {
        Ok(_) => {},
        Err(_) => todo!(),
    }
}
