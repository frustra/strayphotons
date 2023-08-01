use mobile_entry_point::mobile_entry_point;
use std::error::Error;
use winit::{
    event::{Event, WindowEvent},
    event_loop::{ControlFlow, EventLoop},
    window::WindowBuilder,
};

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

    let event_loop = EventLoop::new();

    let _window = WindowBuilder::new()
        .with_title("A fantastic window!")
        .with_inner_size(winit::dpi::LogicalSize::new(128.0, 128.0))
        .build(&event_loop)
        .unwrap();

    event_loop.run(move |event, _, control_flow| match event {
        Event::WindowEvent {
            event: WindowEvent::CloseRequested,
            ..
        } => {
            *control_flow = ControlFlow::Exit;
        }
        _ => (),
    });
}

#[mobile_entry_point]
fn main() {
    match start() {
        Ok(_) | Err(_) => todo!(),
    }
}
