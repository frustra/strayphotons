use mobile_entry_point::mobile_entry_point;
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

    // TODO: Call sp.Start
    // API for setting event_loop (closure to be invoked by c++)
    // API for setting scripts/assets

    Ok(())
}

#[mobile_entry_point]
fn main() {
    match start() {
        Ok(_) | Err(_) => todo!(),
    }
}
