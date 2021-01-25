#![deny(clippy::all)]
//#![forbid(unsafe_code)]

use std::thread;
use log::error;
use pixels::{Pixels, SurfaceTexture};
use winit::dpi::LogicalSize; // FIXME: Get physical size
use winit::event::{Event, VirtualKeyCode};
use winit::event_loop::{ControlFlow, EventLoop};
use winit::window::WindowBuilder;
use winit_input_helper::WinitInputHelper;

#[cxx::bridge(namespace = "sp")]
mod ffi {
    /*#[namespace = "shared"]
    struct Screen {
        height: u32,
        width: u32,
        size: usize,
        frame: &'static mut [u8],
    }*/

    unsafe extern "C++" {
        include!("Api.hh");
        type Api;

        fn connect() -> UniquePtr<Api>;
        fn draw(&self, buf: &mut [u8], size: usize);
    }

    #[namespace = "screen"]
    extern "Rust" {
        fn new_screen(width: u32, height: u32);
    }
}

pub fn new_screen(width: u32, height: u32) {
    thread::spawn(move||{
        env_logger::init();
        let event_loop = EventLoop::new();
        let mut input = WinitInputHelper::new();
        let window = {
            let size = LogicalSize::new(width as f64, height as f64);
            WindowBuilder::new()
                .with_title("Screen")
                .with_inner_size(size)
                .with_min_inner_size(size)
                .build(&event_loop)
                .unwrap()
        };

        let mut pixels = {
            let window_size = window.inner_size();
            let surface_texture = SurfaceTexture::new(window_size.width, window_size.height, &window);
            Pixels::new(window_size.width, window_size.height, surface_texture).unwrap()
        };

        let client = ffi::connect();
        event_loop.run(move |event, _, control_flow| {
            // Draw the current frame
            if let Event::RedrawRequested(_) = event {
                client.draw(pixels.get_frame(), (width * height * 4) as usize); // FIXME: determine size.
                if pixels
                    .render()
                    .map_err(|e| error!("pixels.render() failed: {}", e))
                    .is_err()
                {
                    *control_flow = ControlFlow::Exit;
                    return;
                }
            }
    
            // Handle input events
            if input.update(&event) {
                // Close events
                if input.key_pressed(VirtualKeyCode::Escape) || input.quit() {
                    *control_flow = ControlFlow::Exit;
                    return;
                }
    
                // Resize the window
                if let Some(size) = input.window_resized() {
                    pixels.resize(size.width, size.height);
                }
    
                // Update internal state and request a redraw
                //world.update();
                window.request_redraw();
            }
        });
    });
}