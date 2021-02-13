#![deny(clippy::all)]
//#![forbid(unsafe_code)]

use std::thread;
// use log::error;
use pixels::{Pixels, SurfaceTexture};
use winit::dpi::LogicalSize;
//use winit::event::{Event, VirtualKeyCode};
//use winit::event_loop::ControlFlow;
use winit::event_loop::EventLoop;
use winit::window::WindowBuilder;
//use winit_input_helper::WinitInputHelper;
#[cfg(unix)]
use winit::platform::unix::EventLoopExtUnix;
#[cfg(windows)]
use winit::platform::windows::EventLoopExtWindows;

#[cxx::bridge(namespace = "sp")]
mod ffi {
    /*#[namespace = "shared"] // FIXME: Remove
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
        fn set_frame(&self, buf: &mut [u8]);
    }

    #[namespace = "screen"]
    extern "Rust" {
        unsafe fn new_screen(width: u32, height: u32);
    }
}

pub unsafe fn new_screen(width: u32, height: u32) {
    thread::spawn(move||{
        env_logger::init();
        let event_loop: EventLoop<()> = EventLoop::new_any_thread();
        //let mut input = WinitInputHelper::new();
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
        client.set_frame(pixels.get_frame());
        let _err = pixels.render();
        window.request_redraw();

        /*event_loop.run(move |event: Event<'_, T>, _, control_flow| {
            // Draw the current frame
            if let Event::RedrawRequested(_) = event {
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
        });*/
    });
}