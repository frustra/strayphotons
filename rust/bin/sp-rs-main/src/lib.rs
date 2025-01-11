extern crate sp_sys;
extern crate sp_rs_winit;

use mobile_entry_point::mobile_entry_point;
use sp_sys::{StrayPhotons, Game};
use sp_sys::game::{sp_graphics_ctx_t, sp_window_handlers_t, sp_video_mode_t, sp_graphics_get_winit_context, VkInstance_T, VkSurfaceKHR_T};
use sp_rs_winit::winit;
use std::ffi::{c_char, c_void};
use std::error::Error;
use std::env;
use std::process;
use std::sync::{Mutex, OnceLock};

const MAX_INPUT_POLL_RATE: u32 = 144;

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

unsafe extern fn destroy_vk_instance(graphics: *mut sp_graphics_ctx_t, _vk_instance: *mut VkInstance_T) {
    let winit_context = sp_graphics_get_winit_context(graphics).cast::<winit::WinitContext>().as_mut();
    if let Some(winit_context) = winit_context {
        winit::destroy_instance(winit_context);
    }
}

unsafe extern fn destroy_vk_surface(graphics: *mut sp_graphics_ctx_t, _vk_surface: *mut VkSurfaceKHR_T) {
    let winit_context = sp_graphics_get_winit_context(graphics).cast::<winit::WinitContext>().as_mut();
    if let Some(winit_context) = winit_context {
        winit::destroy_surface(winit_context);
    }
}

unsafe extern fn get_video_modes(graphics: *mut sp_graphics_ctx_t, mode_count_out: *mut usize, modes_out: *mut sp_video_mode_t) {
    let winit_context = sp_graphics_get_winit_context(graphics).cast::<winit::WinitContext>().as_mut();
    if let Some(winit_context) = winit_context {
        assert!(mode_count_out != std::ptr::null_mut(), "windowHandlers.get_video_modes called with null count pointer");

        let modes = winit::get_monitor_modes(winit_context);
        if modes_out != std::ptr::null_mut() && *mode_count_out >= modes.len() {
            for (i, mode) in modes.iter().enumerate() {
                *modes_out.add(i) = sp_video_mode_t {
                    width: mode.width,
                    height: mode.height,
                };
            }
        }
        *mode_count_out = modes.len();
    } else {
        println!("Failed to read Winit monitor modes");
        *mode_count_out = 0;
    }
}

unsafe extern fn set_window_title(graphics: *mut sp_graphics_ctx_t, title: *const c_char) {
    let winit_context = sp_graphics_get_winit_context(graphics).cast::<winit::WinitContext>().as_mut();
    if let Some(winit_context) = winit_context {
        winit::set_window_title(winit_context, title);
    }
}

struct WindowStateCache {
    pub system_fullscreen: bool,
    pub system_window_size: (i32, i32),
    pub stored_window_rect: (i32, i32, i32, i32), // Remember window position and size when returning from fullscreen
}

unsafe extern fn update_window_view(graphics: *mut sp_graphics_ctx_t, width_out: *mut i32, height_out: *mut i32) {
    let winit_context = sp_graphics_get_winit_context(graphics).cast::<winit::WinitContext>().as_mut();
    if let Some(winit_context) = winit_context {
        static STATE_CACHE: OnceLock<Mutex<WindowStateCache>> = OnceLock::new();
        let mut state_cache = STATE_CACHE.get_or_init(|| Mutex::new(WindowStateCache {
            system_fullscreen: false,
            system_window_size: (0, 0),
            stored_window_rect: (0, 0, 0, 0),
        })).lock().unwrap();

        let mut cvar_window_fullscreen = Game::get_cvar("r.fullscreen");
        let mut cvar_window_size = Game::get_cvar("r.windowsize");
        let fullscreen = cvar_window_fullscreen.get_bool();
        if state_cache.system_fullscreen != fullscreen {
            if fullscreen {
                winit::get_window_position(winit_context, &mut state_cache.stored_window_rect.0, &mut state_cache.stored_window_rect.1);
                state_cache.stored_window_rect.2 = state_cache.system_window_size.0;
                state_cache.stored_window_rect.3 = state_cache.system_window_size.1;

                let monitor = winit::get_active_monitor(winit_context);
                let mut read_window_size: (u32, u32) = (0, 0);
                winit::get_monitor_resolution(&*monitor, &mut read_window_size.0, &mut read_window_size.1);
                if read_window_size != (0, 0) {
                    state_cache.system_window_size = (read_window_size.0 as i32, read_window_size.1 as i32);
                }
                winit::set_window_mode(winit_context,
                    &*monitor,
                    0,
                    0,
                    state_cache.system_window_size.0 as u32,
                    state_cache.system_window_size.1 as u32);
            } else {
                state_cache.system_window_size = (state_cache.stored_window_rect.2, state_cache.stored_window_rect.3);
                winit::set_window_mode(winit_context,
                    std::ptr::null(),
                    state_cache.stored_window_rect.0,
                    state_cache.stored_window_rect.1,
                    state_cache.stored_window_rect.2 as u32,
                    state_cache.stored_window_rect.3 as u32);
            }
            cvar_window_size.set_ivec2(state_cache.system_window_size);
            state_cache.system_fullscreen = fullscreen;
        }

        let window_size = cvar_window_size.get_ivec2();
        if state_cache.system_window_size != window_size {
            if cvar_window_fullscreen.get_bool() {
                let monitor = winit::get_active_monitor(winit_context);
                winit::set_window_mode(winit_context, &*monitor, 0, 0, window_size.0 as u32, window_size.1 as u32);
            } else {
                winit::set_window_inner_size(winit_context, window_size.0 as u32, window_size.1 as u32);
            }

            state_cache.system_window_size = window_size;
        }

        let mut framebuffer_extents: (u32, u32) = (0, 0);
        winit::get_window_inner_size(winit_context, &mut framebuffer_extents.0, &mut framebuffer_extents.1);
        if framebuffer_extents.0 > 0 && framebuffer_extents.1 > 0 {
            *width_out = framebuffer_extents.0 as i32;
            *height_out = framebuffer_extents.1 as i32;
        }
    }
}

unsafe extern fn set_cursor_visible(graphics: *mut sp_graphics_ctx_t, visible: u32) {
    let winit_context = sp_graphics_get_winit_context(graphics).cast::<winit::WinitContext>().as_mut();
    if let Some(winit_context) = winit_context {
        if visible != 0 {
            winit::set_input_mode(winit_context, winit::InputMode::CursorNormal);
        } else {
            winit::set_input_mode(winit_context, winit::InputMode::CursorDisabled);
        }
    }
}

fn run_until_exit() -> Result<(), Box<dyn Error>> {
    init_logging()?;
    let current_dir = env::current_dir()?;
    log::info!("Rust working directory: {}", current_dir.display());

    unsafe {
        let mut sp = StrayPhotons::init(&vec![]);
        let mut game: Game = sp.game();
        let mut graphics = game.get_graphics_context();

        let mut window_size_cvar = Game::get_cvar("r.windowsize");
        let initial_size: (i32, i32) = window_size_cvar.get_ivec2();
        let enable_validation_layers = game.get_cli_flag("with-validation-layers");
        winit::create_context(game.into(), initial_size.0, initial_size.1, enable_validation_layers);
        let winit_context = graphics.get_winit_context().cast::<winit::WinitContext>().as_mut().expect("Rust.get_winit_context() returned null WinitContext");

        let vk_instance = winit::get_instance_handle(winit_context) as usize;
        assert!(vk_instance != 0, "winit instance creation failed");
        graphics.set_vulkan_instance(vk_instance, destroy_vk_instance);

        let vk_surface = winit::get_surface_handle(winit_context) as usize;
        assert!(vk_surface != 0, "winit window surface creation failed");
        graphics.set_vulkan_surface(vk_surface, destroy_vk_surface);

        let window_handlers = sp_window_handlers_t {
            get_video_modes: Some(get_video_modes),
            set_title: Some(set_window_title),
            should_close: None,
            update_window_view: Some(update_window_view),
            set_cursor_visible: Some(set_cursor_visible),
            win32_handle: winit::get_win32_window_handle(winit_context) as *mut c_void,
        };
        graphics.set_window_handlers(&window_handlers);

        let mut status = game.start();
        if status == 0 {
            winit::start_event_loop(game.into(), MAX_INPUT_POLL_RATE);
            status = game.get_exit_code();
        }
        if status != 0 {
            eprintln!("Stray Photons exited with code: {}", status);
        }
        drop(sp);
        process::exit(status);
    }
}

#[mobile_entry_point]
fn main() {
    match run_until_exit() {
        Ok(_) => {},
        Err(err) => {
            eprintln!("Error: {}", err);
        },
    }
}
