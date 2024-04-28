/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

use cxx::CxxString;
use std::sync::Arc;
#[cfg(not(any(target_os = "android", target_os = "ios")))]
use std::time::Duration;
use vulkano::{
    instance::{
        debug::{
            DebugUtilsMessageSeverity, DebugUtilsMessageType, DebugUtilsMessengerCallback,
            DebugUtilsMessengerCreateInfo,
        },
        Instance, InstanceCreateInfo,
    },
    swapchain::Surface,
    Handle, VulkanLibrary, VulkanObject,
};
use winit::{
    dpi::{PhysicalPosition, PhysicalSize},
    event::{DeviceEvent, ElementState, Event, MouseScrollDelta, WindowEvent},
    event_loop::{EventLoop, EventLoopBuilder, ControlFlow},
    monitor::{MonitorHandle, VideoMode},
    window::WindowBuilder,
    window::{CursorGrabMode, Fullscreen, Window},
};

#[cxx::bridge(namespace = "sp")]
mod ctx {
    #[repr(i32)]
    #[derive(Debug, Clone, Copy)]
    pub enum KeyCode {
        KEY_INVALID = 0,
        KEY_SPACE = 32,
        KEY_APOSTROPHE = 39,
        KEY_COMMA = 44,
        KEY_MINUS = 45,
        KEY_PERIOD = 46,
        KEY_SLASH = 47,
        KEY_0 = 48,
        KEY_1 = 49,
        KEY_2 = 50,
        KEY_3 = 51,
        KEY_4 = 52,
        KEY_5 = 53,
        KEY_6 = 54,
        KEY_7 = 55,
        KEY_8 = 56,
        KEY_9 = 57,
        KEY_SEMICOLON = 59,
        KEY_EQUALS = 61,
        KEY_A = 65,
        KEY_B = 66,
        KEY_C = 67,
        KEY_D = 68,
        KEY_E = 69,
        KEY_F = 70,
        KEY_G = 71,
        KEY_H = 72,
        KEY_I = 73,
        KEY_J = 74,
        KEY_K = 75,
        KEY_L = 76,
        KEY_M = 77,
        KEY_N = 78,
        KEY_O = 79,
        KEY_P = 80,
        KEY_Q = 81,
        KEY_R = 82,
        KEY_S = 83,
        KEY_T = 84,
        KEY_U = 85,
        KEY_V = 86,
        KEY_W = 87,
        KEY_X = 88,
        KEY_Y = 89,
        KEY_Z = 90,
        KEY_LEFT_BRACKET = 91,
        KEY_BACKSLASH = 92,
        KEY_RIGHT_BRACKET = 93,
        KEY_BACKTICK = 96,
        KEY_ESCAPE = 256,
        KEY_ENTER = 257,
        KEY_TAB = 258,
        KEY_BACKSPACE = 259,
        KEY_INSERT = 260,
        KEY_DELETE = 261,
        KEY_RIGHT_ARROW = 262,
        KEY_LEFT_ARROW = 263,
        KEY_DOWN_ARROW = 264,
        KEY_UP_ARROW = 265,
        KEY_PAGE_UP = 266,
        KEY_PAGE_DOWN = 267,
        KEY_HOME = 268,
        KEY_END = 269,
        KEY_CAPS_LOCK = 280,
        KEY_SCROLL_LOCK = 281,
        KEY_NUM_LOCK = 282,
        KEY_PRINT_SCREEN = 283,
        KEY_PAUSE = 284,
        KEY_F1 = 290,
        KEY_F2 = 291,
        KEY_F3 = 292,
        KEY_F4 = 293,
        KEY_F5 = 294,
        KEY_F6 = 295,
        KEY_F7 = 296,
        KEY_F8 = 297,
        KEY_F9 = 298,
        KEY_F10 = 299,
        KEY_F11 = 300,
        KEY_F12 = 301,
        KEY_F13 = 302,
        KEY_F14 = 303,
        KEY_F15 = 304,
        KEY_F16 = 305,
        KEY_F17 = 306,
        KEY_F18 = 307,
        KEY_F19 = 308,
        KEY_F20 = 309,
        KEY_F21 = 310,
        KEY_F22 = 311,
        KEY_F23 = 312,
        KEY_F24 = 313,
        KEY_F25 = 314,
        KEY_0_NUMPAD = 320,
        KEY_1_NUMPAD = 321,
        KEY_2_NUMPAD = 322,
        KEY_3_NUMPAD = 323,
        KEY_4_NUMPAD = 324,
        KEY_5_NUMPAD = 325,
        KEY_6_NUMPAD = 326,
        KEY_7_NUMPAD = 327,
        KEY_8_NUMPAD = 328,
        KEY_9_NUMPAD = 329,
        KEY_DECIMAL_NUMPAD = 330,
        KEY_DIVIDE_NUMPAD = 331,
        KEY_MULTIPLY_NUMPAD = 332,
        KEY_MINUS_NUMPAD = 333,
        KEY_PLUS_NUMPAD = 334,
        KEY_ENTER_NUMPAD = 335,
        KEY_EQUALS_NUMPAD = 336,
        KEY_LEFT_SHIFT = 340,
        KEY_LEFT_CONTROL = 341,
        KEY_LEFT_ALT = 342,
        KEY_LEFT_SUPER = 343,
        KEY_RIGHT_SHIFT = 344,
        KEY_RIGHT_CONTROL = 345,
        KEY_RIGHT_ALT = 346,
        KEY_RIGHT_SUPER = 347,
        KEY_CONTEXT_MENU = 348,
    }

    #[repr(i32)]
    #[derive(Debug, Clone, Copy)]
    pub enum InputAction {
        RELEASE = 0,
        PRESS = 1,
        REPEAT = 2,
    }

    #[repr(i32)]
    #[derive(Debug, Clone, Copy)]
    pub enum MouseButton {
        BUTTON_LEFT = 0,
        BUTTON_RIGHT = 1,
        BUTTON_MIDDLE = 2,
        BUTTON_4 = 3,
        BUTTON_5 = 4,
        BUTTON_6 = 5,
        BUTTON_7 = 6,
        BUTTON_8 = 7,
    }

    #[derive(Debug, Clone, Copy)]
    pub enum InputMode {
        CursorDisabled,
        CursorNormal,
    }

    #[derive(Debug, Clone, Copy)]
    pub struct MonitorMode {
        width: u32,
        height: u32,
        bit_depth: u16,
        refresh_rate_millihertz: u32,
    }

    unsafe extern "C++" {
        include!("input/KeyCodes.hh");
        include!("sp-rs/include/InputCallbacks.hh");
        type KeyCode;
        type InputAction;
        type MouseButton;
        type WinitInputHandler;

        #[cfg(not(any(target_os = "android", target_os = "ios")))]
        unsafe fn InputFrameCallback(ctx: *mut WinitInputHandler) -> bool;
        #[cfg(not(any(target_os = "android", target_os = "ios")))]
        unsafe fn KeyInputCallback(
            ctx: *mut WinitInputHandler,
            key: KeyCode,
            scancode: i32,
            action: InputAction,
        );
        #[cfg(not(any(target_os = "android", target_os = "ios")))]
        unsafe fn CharInputCallback(ctx: *mut WinitInputHandler, ch: u32);
        unsafe fn MouseMoveCallback(ctx: *mut WinitInputHandler, dx: f64, dy: f64);
        unsafe fn MousePositionCallback(ctx: *mut WinitInputHandler, xPos: f64, yPos: f64);
        unsafe fn MouseButtonCallback(
            ctx: *mut WinitInputHandler,
            button: MouseButton,
            action: InputAction,
        );
        unsafe fn MouseScrollCallback(ctx: *mut WinitInputHandler, xOffset: f64, yOffset: f64);
    }

    #[namespace = "sp::winit"]
    extern "Rust" {
        type WinitContext;
        type MonitorContext;
        fn create_context(width: i32, height: i32, enable_validation: bool) -> Box<WinitContext>;
        fn create_view(context: &mut WinitContext);
        fn destroy_window(context: &mut WinitContext);
        fn destroy_surface(context: &mut WinitContext);
        fn destroy_instance(context: &mut WinitContext);

        fn get_surface_handle(context: &WinitContext) -> u64;
        fn get_instance_handle(context: &WinitContext) -> u64;
        fn get_win32_window_handle(_context: &WinitContext) -> u64;

        fn get_monitor_modes(context: &WinitContext) -> Vec<MonitorMode>;
        fn get_active_monitor(context: &WinitContext) -> Box<MonitorContext>;
        unsafe fn get_monitor_resolution(
            monitor: &MonitorContext,
            width_out: *mut u32,
            height_out: *mut u32,
        );
        unsafe fn get_window_position(context: &WinitContext, x_out: *mut i32, y_out: *mut i32);
        unsafe fn get_window_inner_size(
            context: &WinitContext,
            width_out: *mut u32,
            height_out: *mut u32,
        );

        fn set_window_title(context: &WinitContext, title: &CxxString);
        unsafe fn set_window_mode(
            context: &WinitContext,
            monitor: *const MonitorContext,
            pos_x: i32,
            pos_y: i32,
            width: u32,
            height: u32,
        );
        fn set_window_inner_size(context: &WinitContext, width: u32, height: u32);
        fn set_input_mode(context: &WinitContext, mode: InputMode);
        unsafe fn start_event_loop(
            context: &mut WinitContext,
            ctx: *mut WinitInputHandler,
            _max_input_rate: u32,
        );
    }
}

use crate::winit::ctx::*;

pub struct WinitContext {
    pub library: Arc<VulkanLibrary>,
    pub instance: Option<Arc<Instance>>,
    initial_width: i32,
    initial_height: i32,
    pub window: Option<Arc<Window>>,
    pub surface: Option<Arc<Surface>>,
    pub event_loop: Option<EventLoop<()>>,
}

unsafe impl Send for WinitContext {}
unsafe impl Sync for WinitContext {}

pub struct MonitorContext {
    pub handle: Option<MonitorHandle>,
}

#[cfg(target_os = "android")]
#[no_mangle]
pub static APP: std::sync::OnceLock<winit::platform::android::activity::AndroidApp> =
    std::sync::OnceLock::new();

fn create_view(context: &mut WinitContext) {
    let window: Arc<Window> = Arc::new(
        WindowBuilder::new()
            .with_title("STRAY PHOTONS")
            .with_resizable(false)
            .with_inner_size(PhysicalSize {
                width: context.initial_width,
                height: context.initial_height,
            })
            .build(&context.event_loop.as_ref().unwrap())
            .expect("Failed to create window"),
    );

    let surface = Surface::from_window(context.instance.clone().unwrap(), window.clone()).unwrap();

    context.window = Some(window.to_owned());
    context.surface = Some(surface.to_owned());
}

fn create_context(width: i32, height: i32, enable_validation: bool) -> Box<WinitContext> {
    let library: Arc<VulkanLibrary> = VulkanLibrary::new().expect("no local Vulkan library/DLL");

    #[cfg(not(target_os = "android"))]
    let event_loop = EventLoopBuilder::new().build().unwrap();

    #[cfg(target_os = "android")]
    let event_loop = {
        use winit::platform::android::EventLoopBuilderExtAndroid;
        EventLoopBuilder::new()
            .with_android_app(APP.get().unwrap().clone())
            .build()
            .unwrap()
    };

    // TODO: Match these extensions with C++ DeviceContext.cc
    let mut required_extensions: vulkano::instance::InstanceExtensions =
        Surface::required_extensions(&event_loop);
    required_extensions.khr_surface = true;
    required_extensions.khr_get_physical_device_properties2 = true;
    required_extensions.ext_debug_utils = true;

    let mut enabled_layers = Vec::<String>::new();
    if enable_validation {
        println!("Running with Vulkan validation layer");
        enabled_layers.push("VK_LAYER_KHRONOS_validation".to_string());
    }

    // TODO: Match these settings with C++ DeviceContext.cc
    let debug_create_info = unsafe {
        DebugUtilsMessengerCreateInfo {
            message_severity: DebugUtilsMessageSeverity::ERROR | DebugUtilsMessageSeverity::WARNING,
            message_type: DebugUtilsMessageType::GENERAL
                | DebugUtilsMessageType::PERFORMANCE
                | DebugUtilsMessageType::VALIDATION,
            ..DebugUtilsMessengerCreateInfo::user_callback(DebugUtilsMessengerCallback::new(
                |message_severity, message_type, callback_data| {
                    let severity = if message_severity.intersects(DebugUtilsMessageSeverity::ERROR)
                    {
                        "error"
                    } else if message_severity.intersects(DebugUtilsMessageSeverity::WARNING) {
                        "warning"
                    } else if message_severity.intersects(DebugUtilsMessageSeverity::INFO) {
                        "information"
                    } else if message_severity.intersects(DebugUtilsMessageSeverity::VERBOSE) {
                        "verbose"
                    } else {
                        ""
                    };

                    let ty = if message_type.intersects(DebugUtilsMessageType::GENERAL) {
                        "general"
                    } else if message_type.intersects(DebugUtilsMessageType::VALIDATION) {
                        "validation"
                    } else if message_type.intersects(DebugUtilsMessageType::PERFORMANCE) {
                        "performance"
                    } else {
                        ""
                    };

                    println!(
                        "{} {} {}: {}",
                        callback_data.message_id_name.unwrap_or("unknown"),
                        ty,
                        severity,
                        callback_data.message
                    );
                },
            ))
        }
    };

    let instance: Arc<Instance> = Instance::new(
        library.clone(),
        InstanceCreateInfo {
            enabled_extensions: required_extensions,
            enabled_layers: enabled_layers,
            debug_utils_messengers: vec![debug_create_info],
            ..Default::default()
        },
    )
    .expect("failed to create instance");

    let mut context = WinitContext {
        library: library.to_owned(),
        instance: Some(instance.to_owned()),
        initial_width: width,
        initial_height: height,
        window: None,
        surface: None,
        event_loop: Some(event_loop),
    };

    #[cfg(not(any(target_os = "android", target_os = "ios")))]
    create_view(&mut context);

    println!("Hello, Rust!");
    Box::new(context)
}

fn destroy_window(context: &mut WinitContext) {
    context.window.take().unwrap();
}

fn destroy_surface(context: &mut WinitContext) {
    context.surface.take().unwrap();
}

fn destroy_instance(context: &mut WinitContext) {
    context.instance.take().unwrap();
}

fn get_surface_handle(context: &WinitContext) -> u64 {
    println!("Library API version: {}", context.library.api_version());
    if let Some(instance) = context.instance.as_ref() {
        println!("Instance API version: {}", instance.api_version());
    }
    context
        .surface
        .as_ref()
        .map_or(0u64, |s| s.handle().as_raw())
}

fn get_instance_handle(context: &WinitContext) -> u64 {
    context
        .instance
        .as_ref()
        .map_or(0u64, |s| s.handle().as_raw())
}

#[cfg(target_os = "windows")]
fn get_win32_window_handle(_context: &WinitContext) -> u64 {
    use raw_window_handle::HasRawWindowHandle;
    use raw_window_handle::RawWindowHandle;
    if let Some(window) = _context.window.as_ref() {
        match window.raw_window_handle() {
            RawWindowHandle::Win32(handle) => handle.hwnd as u64,
            _ => 0u64,
        }
    } else {
        0u64
    }
}

#[cfg(not(target_os = "windows"))]
fn get_win32_window_handle(_context: &WinitContext) -> u64 {
    0u64
}

fn get_monitor_modes(context: &WinitContext) -> Vec<MonitorMode> {
    if let Some(window) = context.window.as_ref() {
        if let Some(monitor) = window.current_monitor().as_ref() {
            monitor
                .video_modes()
                .map(|mode| {
                    let size = mode.size();
                    MonitorMode {
                        width: size.width,
                        height: size.height,
                        bit_depth: mode.bit_depth(),
                        refresh_rate_millihertz: mode.refresh_rate_millihertz(),
                    }
                })
                .collect()
        } else {
            vec![]
        }
    } else {
        vec![]
    }
}

fn get_active_monitor(context: &WinitContext) -> Box<MonitorContext> {
    let mut handle: Option<MonitorHandle> = None;
    if let Some(window) = context.window.as_ref() {
        handle = window
            .current_monitor()
            .or(window.primary_monitor())
            .or(window.available_monitors().nth(0));
    }
    return Box::new(MonitorContext { handle });
}

unsafe fn get_monitor_resolution(
    monitor: &MonitorContext,
    width_out: *mut u32,
    height_out: *mut u32,
) {
    let window_size = monitor
        .handle
        .as_ref()
        .map(|h| h.size())
        .unwrap_or(PhysicalSize::new(0, 0));
    *width_out = window_size.width;
    *height_out = window_size.height;
}

unsafe fn get_window_position(context: &WinitContext, x_out: *mut i32, y_out: *mut i32) {
    if let Some(window) = context.window.as_ref() {
        let window_pos = window
            .outer_position()
            .unwrap_or(PhysicalPosition::new(i32::MIN, i32::MIN));
        *x_out = window_pos.x;
        *y_out = window_pos.y;
    } else {
        *x_out = i32::MIN;
        *y_out = i32::MIN;
    }
}

unsafe fn get_window_inner_size(context: &WinitContext, width_out: *mut u32, height_out: *mut u32) {
    if let Some(window) = context.window.as_ref() {
        let window_size = window.inner_size();
        *width_out = window_size.width;
        *height_out = window_size.height;
    } else {
        *width_out = 0;
        *height_out = 0;
    }
}

fn set_window_title(context: &WinitContext, title: &CxxString) {
    if let Some(window) = context.window.as_ref() {
        window.set_title(title.to_str().unwrap_or("error"));
    }
}

unsafe fn set_window_mode(
    context: &WinitContext,
    monitor: *const MonitorContext,
    pos_x: i32,
    pos_y: i32,
    width: u32,
    height: u32,
) {
    if let Some(window) = context.window.as_ref() {
        if let Some(monitor) = monitor.as_ref() {
            // Fullscreen mode
            if let Some(handle) = monitor.handle.as_ref() {
                let mut modes: Vec<VideoMode> = handle
                    .video_modes()
                    .filter(|mode| {
                        let size = mode.size();
                        return size.width == width && size.height == height;
                    })
                    .collect();
                modes.sort_by(|a, b| {
                    (a.bit_depth(), -(a.refresh_rate_millihertz() as i64))
                        .cmp(&(b.bit_depth(), -(b.refresh_rate_millihertz() as i64)))
                });
                if let Some(mode) = modes.first() {
                    window.set_fullscreen(Some(Fullscreen::Exclusive(mode.clone())));
                }
            }
        } else {
            // Window mode
            window.set_fullscreen(None);
            window.set_outer_position(PhysicalPosition::new(pos_x, pos_y));
            let _ = window.request_inner_size(PhysicalSize::new(width, height));
        }
    }
}

fn set_window_inner_size(context: &WinitContext, width: u32, height: u32) {
    if let Some(window) = context.window.as_ref() {
        let _ = window.request_inner_size(PhysicalSize::new(width, height));
    }
}

fn set_input_mode(context: &WinitContext, mode: InputMode) {
    if let Some(window) = context.window.as_ref() {
        match mode {
            InputMode::CursorDisabled => {
                window.set_cursor_grab(CursorGrabMode::Confined).unwrap();
                window.set_cursor_visible(false);
            }
            InputMode::CursorNormal => {
                window.set_cursor_grab(CursorGrabMode::None).unwrap();
                window.set_cursor_visible(true);
            }
            _ => (),
        }
    }
}

fn start_event_loop(
    context: &mut WinitContext,
    input_handler: *mut WinitInputHandler,
    _max_input_rate: u32,
) {
    let event_loop = context.event_loop.take().unwrap();
    event_loop
        .run(move |event, loop_target| {
            match event {
                #[cfg(target_os = "android")]
                Event::Resumed => {
                    create_view(context);
                }
                #[cfg(target_os = "android")]
                Event::Suspended => {
                    log::info!("Suspended, dropping render state...");
                }
                Event::DeviceEvent { event, .. } => match event {
                    // TODO: Map unique device_ids to different events/signals
                    DeviceEvent::MouseMotion { delta } => unsafe {
                        MouseMoveCallback(input_handler, delta.0, delta.1);
                    },
                    _ => (),
                },
                Event::WindowEvent { event, .. } => match event {
                    WindowEvent::CloseRequested => {
                        println!("Exit requested by Winit");
                        loop_target.exit();
                    }
                    WindowEvent::CursorMoved { position, .. } => unsafe {
                        MousePositionCallback(input_handler, position.x, position.y);
                    },
                    WindowEvent::MouseWheel {
                        delta: MouseScrollDelta::LineDelta(dx, dy),
                        // TODO: Support PixelDelta variant
                        ..
                    } => unsafe {
                        MouseScrollCallback(input_handler, dx as f64, dy as f64);
                    },
                    WindowEvent::MouseInput { state, button, .. } => {
                        let action: InputAction = match state {
                            ElementState::Released => InputAction::RELEASE,
                            ElementState::Pressed => InputAction::PRESS,
                        };
                        let btn: MouseButton = match button {
                            winit::event::MouseButton::Left => MouseButton::BUTTON_LEFT,
                            winit::event::MouseButton::Right => MouseButton::BUTTON_RIGHT,
                            winit::event::MouseButton::Middle => MouseButton::BUTTON_MIDDLE,
                            winit::event::MouseButton::Other(3) => MouseButton::BUTTON_4,
                            winit::event::MouseButton::Other(4) => MouseButton::BUTTON_5,
                            winit::event::MouseButton::Other(5) => MouseButton::BUTTON_6,
                            winit::event::MouseButton::Other(6) => MouseButton::BUTTON_7,
                            winit::event::MouseButton::Other(7) => MouseButton::BUTTON_8,
                            _ => MouseButton::BUTTON_LEFT,
                        };
                        unsafe {
                            MouseButtonCallback(input_handler, btn, action);
                        }
                    }
                    #[cfg(not(any(target_os = "android", target_os = "ios")))]
                    WindowEvent::KeyboardInput { event, .. } => {
                        use winit::platform::scancode::PhysicalKeyExtScancode;
                        let key: KeyCode = into_keycode(event.physical_key);
                        let scancode: u32 = event.physical_key.to_scancode().unwrap_or(0u32);
                        let mut action: InputAction = match event.state {
                            ElementState::Released => InputAction::RELEASE,
                            ElementState::Pressed => InputAction::PRESS,
                        };
                        if event.repeat && action == InputAction::PRESS {
                            action = InputAction::REPEAT;
                        }
                        unsafe {
                            KeyInputCallback(input_handler, key, scancode as i32, action);
                        }

                        if let Some(text) = event.text {
                            for ch in text.chars() {
                                unsafe {
                                    CharInputCallback(input_handler, ch as u32);
                                }
                            }
                        }
                    }
                    _ => (),
                },
                #[cfg(not(any(target_os = "android", target_os = "ios")))]
                Event::AboutToWait => {
                    let should_close: bool;
                    unsafe {
                        should_close = !InputFrameCallback(input_handler);
                    }
                    if should_close {
                        loop_target.exit();
                    } else if _max_input_rate > 0 {
                        loop_target.set_control_flow(ControlFlow::wait_duration(Duration::from_secs(1) / _max_input_rate));
                    } else {
                        loop_target.set_control_flow(ControlFlow::Wait);
                    }
                }
                _ => (),
            }
        })
        .unwrap();
}

#[cfg(not(any(target_os = "android", target_os = "ios")))]
fn into_keycode(winit_key: winit::keyboard::PhysicalKey) -> KeyCode {
    if let winit::keyboard::PhysicalKey::Code(winit_keycode) = winit_key {
        return match winit_keycode {
            winit::keyboard::KeyCode::Backquote => KeyCode::KEY_BACKTICK,
            winit::keyboard::KeyCode::Backslash => KeyCode::KEY_BACKSLASH,
            winit::keyboard::KeyCode::BracketLeft => KeyCode::KEY_LEFT_BRACKET,
            winit::keyboard::KeyCode::BracketRight => KeyCode::KEY_RIGHT_BRACKET,
            winit::keyboard::KeyCode::Comma => KeyCode::KEY_COMMA,
            winit::keyboard::KeyCode::Digit0 => KeyCode::KEY_0,
            winit::keyboard::KeyCode::Digit1 => KeyCode::KEY_1,
            winit::keyboard::KeyCode::Digit2 => KeyCode::KEY_2,
            winit::keyboard::KeyCode::Digit3 => KeyCode::KEY_3,
            winit::keyboard::KeyCode::Digit4 => KeyCode::KEY_4,
            winit::keyboard::KeyCode::Digit5 => KeyCode::KEY_5,
            winit::keyboard::KeyCode::Digit6 => KeyCode::KEY_6,
            winit::keyboard::KeyCode::Digit7 => KeyCode::KEY_7,
            winit::keyboard::KeyCode::Digit8 => KeyCode::KEY_8,
            winit::keyboard::KeyCode::Digit9 => KeyCode::KEY_9,
            winit::keyboard::KeyCode::Equal => KeyCode::KEY_EQUALS,
            // IntlBackslash
            // IntlRo
            // IntlYen
            winit::keyboard::KeyCode::KeyA => KeyCode::KEY_A,
            winit::keyboard::KeyCode::KeyB => KeyCode::KEY_B,
            winit::keyboard::KeyCode::KeyC => KeyCode::KEY_C,
            winit::keyboard::KeyCode::KeyD => KeyCode::KEY_D,
            winit::keyboard::KeyCode::KeyE => KeyCode::KEY_E,
            winit::keyboard::KeyCode::KeyF => KeyCode::KEY_F,
            winit::keyboard::KeyCode::KeyG => KeyCode::KEY_G,
            winit::keyboard::KeyCode::KeyH => KeyCode::KEY_H,
            winit::keyboard::KeyCode::KeyI => KeyCode::KEY_I,
            winit::keyboard::KeyCode::KeyJ => KeyCode::KEY_J,
            winit::keyboard::KeyCode::KeyK => KeyCode::KEY_K,
            winit::keyboard::KeyCode::KeyL => KeyCode::KEY_L,
            winit::keyboard::KeyCode::KeyM => KeyCode::KEY_M,
            winit::keyboard::KeyCode::KeyN => KeyCode::KEY_N,
            winit::keyboard::KeyCode::KeyO => KeyCode::KEY_O,
            winit::keyboard::KeyCode::KeyP => KeyCode::KEY_P,
            winit::keyboard::KeyCode::KeyQ => KeyCode::KEY_Q,
            winit::keyboard::KeyCode::KeyR => KeyCode::KEY_R,
            winit::keyboard::KeyCode::KeyS => KeyCode::KEY_S,
            winit::keyboard::KeyCode::KeyT => KeyCode::KEY_T,
            winit::keyboard::KeyCode::KeyU => KeyCode::KEY_U,
            winit::keyboard::KeyCode::KeyV => KeyCode::KEY_V,
            winit::keyboard::KeyCode::KeyW => KeyCode::KEY_W,
            winit::keyboard::KeyCode::KeyX => KeyCode::KEY_X,
            winit::keyboard::KeyCode::KeyY => KeyCode::KEY_Y,
            winit::keyboard::KeyCode::KeyZ => KeyCode::KEY_Z,
            winit::keyboard::KeyCode::Minus => KeyCode::KEY_MINUS,
            winit::keyboard::KeyCode::Period => KeyCode::KEY_PERIOD,
            winit::keyboard::KeyCode::Quote => KeyCode::KEY_APOSTROPHE,
            winit::keyboard::KeyCode::Semicolon => KeyCode::KEY_SEMICOLON,
            winit::keyboard::KeyCode::Slash => KeyCode::KEY_SLASH,
            winit::keyboard::KeyCode::AltLeft => KeyCode::KEY_LEFT_ALT,
            winit::keyboard::KeyCode::AltRight => KeyCode::KEY_RIGHT_ALT,
            winit::keyboard::KeyCode::Backspace => KeyCode::KEY_BACKSPACE,
            winit::keyboard::KeyCode::CapsLock => KeyCode::KEY_CAPS_LOCK,
            winit::keyboard::KeyCode::ContextMenu => KeyCode::KEY_CONTEXT_MENU,
            winit::keyboard::KeyCode::ControlLeft => KeyCode::KEY_LEFT_CONTROL,
            winit::keyboard::KeyCode::ControlRight => KeyCode::KEY_RIGHT_CONTROL,
            winit::keyboard::KeyCode::Enter => KeyCode::KEY_ENTER,
            winit::keyboard::KeyCode::SuperLeft => KeyCode::KEY_LEFT_SUPER,
            winit::keyboard::KeyCode::SuperRight => KeyCode::KEY_RIGHT_SUPER,
            winit::keyboard::KeyCode::ShiftLeft => KeyCode::KEY_LEFT_SHIFT,
            winit::keyboard::KeyCode::ShiftRight => KeyCode::KEY_RIGHT_SHIFT,
            winit::keyboard::KeyCode::Space => KeyCode::KEY_SPACE,
            winit::keyboard::KeyCode::Tab => KeyCode::KEY_TAB,
            // Convert
            // KanaMode
            // Lang1
            // Lang2
            // Lang3
            // Lang4
            // NonConvert
            winit::keyboard::KeyCode::Delete => KeyCode::KEY_DELETE,
            winit::keyboard::KeyCode::End => KeyCode::KEY_END,
            // Help
            winit::keyboard::KeyCode::Home => KeyCode::KEY_HOME,
            winit::keyboard::KeyCode::Insert => KeyCode::KEY_INSERT,
            winit::keyboard::KeyCode::PageDown => KeyCode::KEY_PAGE_DOWN,
            winit::keyboard::KeyCode::PageUp => KeyCode::KEY_PAGE_UP,
            winit::keyboard::KeyCode::ArrowDown => KeyCode::KEY_DOWN_ARROW,
            winit::keyboard::KeyCode::ArrowLeft => KeyCode::KEY_LEFT_ARROW,
            winit::keyboard::KeyCode::ArrowRight => KeyCode::KEY_RIGHT_ARROW,
            winit::keyboard::KeyCode::ArrowUp => KeyCode::KEY_UP_ARROW,
            winit::keyboard::KeyCode::NumLock => KeyCode::KEY_NUM_LOCK,
            winit::keyboard::KeyCode::Numpad0 => KeyCode::KEY_0_NUMPAD,
            winit::keyboard::KeyCode::Numpad1 => KeyCode::KEY_1_NUMPAD,
            winit::keyboard::KeyCode::Numpad2 => KeyCode::KEY_2_NUMPAD,
            winit::keyboard::KeyCode::Numpad3 => KeyCode::KEY_3_NUMPAD,
            winit::keyboard::KeyCode::Numpad4 => KeyCode::KEY_4_NUMPAD,
            winit::keyboard::KeyCode::Numpad5 => KeyCode::KEY_5_NUMPAD,
            winit::keyboard::KeyCode::Numpad6 => KeyCode::KEY_6_NUMPAD,
            winit::keyboard::KeyCode::Numpad7 => KeyCode::KEY_7_NUMPAD,
            winit::keyboard::KeyCode::Numpad8 => KeyCode::KEY_8_NUMPAD,
            winit::keyboard::KeyCode::Numpad9 => KeyCode::KEY_9_NUMPAD,
            winit::keyboard::KeyCode::NumpadAdd => KeyCode::KEY_PLUS_NUMPAD,
            // NumpadBackspace
            // NumpadClear
            // NumpadClearEntry
            // NumpadComma
            winit::keyboard::KeyCode::NumpadDecimal => KeyCode::KEY_DECIMAL_NUMPAD,
            winit::keyboard::KeyCode::NumpadDivide => KeyCode::KEY_DIVIDE_NUMPAD,
            winit::keyboard::KeyCode::NumpadEnter => KeyCode::KEY_ENTER_NUMPAD,
            winit::keyboard::KeyCode::NumpadEqual => KeyCode::KEY_EQUALS_NUMPAD,
            // NumpadHash
            // NumpadMemoryAdd
            // NumpadMemoryClear
            // NumpadMemoryRecall
            // NumpadMemoryStore
            // NumpadMemorySubtract
            winit::keyboard::KeyCode::NumpadMultiply => KeyCode::KEY_MULTIPLY_NUMPAD,
            // NumpadParenLeft
            // NumpadParenRight
            // NumpadStar
            winit::keyboard::KeyCode::NumpadSubtract => KeyCode::KEY_MINUS_NUMPAD,
            winit::keyboard::KeyCode::Escape => KeyCode::KEY_ESCAPE,
            // Fn
            // FnLock
            winit::keyboard::KeyCode::PrintScreen => KeyCode::KEY_PRINT_SCREEN,
            winit::keyboard::KeyCode::ScrollLock => KeyCode::KEY_SCROLL_LOCK,
            winit::keyboard::KeyCode::Pause => KeyCode::KEY_PAUSE,
            // BrowserBack
            // BrowserFavorites
            // BrowserForward
            // BrowserHome
            // BrowserRefresh
            // BrowserSearch
            // BrowserStop
            // Eject
            // LaunchApp1
            // LaunchApp2
            // LaunchMail
            // MediaPlayPause
            // MediaSelect
            // MediaStop
            // MediaTrackNext
            // MediaTrackPrevious
            // Power
            // Sleep
            // AudioVolumeDown
            // AudioVolumeMute
            // AudioVolumeUp
            // WakeUp
            // Meta
            // Hyper
            // Turbo
            // Abort
            // Resume
            // Suspend
            // Again
            // Copy
            // Cut
            // Find
            // Open
            // Paste
            // Props
            // Select
            // Undo
            // Hiragana
            // Katakana
            winit::keyboard::KeyCode::F1 => KeyCode::KEY_F1,
            winit::keyboard::KeyCode::F2 => KeyCode::KEY_F2,
            winit::keyboard::KeyCode::F3 => KeyCode::KEY_F3,
            winit::keyboard::KeyCode::F4 => KeyCode::KEY_F4,
            winit::keyboard::KeyCode::F5 => KeyCode::KEY_F5,
            winit::keyboard::KeyCode::F6 => KeyCode::KEY_F6,
            winit::keyboard::KeyCode::F7 => KeyCode::KEY_F7,
            winit::keyboard::KeyCode::F8 => KeyCode::KEY_F8,
            winit::keyboard::KeyCode::F9 => KeyCode::KEY_F9,
            winit::keyboard::KeyCode::F10 => KeyCode::KEY_F10,
            winit::keyboard::KeyCode::F11 => KeyCode::KEY_F11,
            winit::keyboard::KeyCode::F12 => KeyCode::KEY_F12,
            winit::keyboard::KeyCode::F13 => KeyCode::KEY_F13,
            winit::keyboard::KeyCode::F14 => KeyCode::KEY_F14,
            winit::keyboard::KeyCode::F15 => KeyCode::KEY_F15,
            winit::keyboard::KeyCode::F16 => KeyCode::KEY_F16,
            winit::keyboard::KeyCode::F17 => KeyCode::KEY_F17,
            winit::keyboard::KeyCode::F18 => KeyCode::KEY_F18,
            winit::keyboard::KeyCode::F19 => KeyCode::KEY_F19,
            winit::keyboard::KeyCode::F20 => KeyCode::KEY_F20,
            winit::keyboard::KeyCode::F21 => KeyCode::KEY_F21,
            winit::keyboard::KeyCode::F22 => KeyCode::KEY_F22,
            winit::keyboard::KeyCode::F23 => KeyCode::KEY_F23,
            winit::keyboard::KeyCode::F24 => KeyCode::KEY_F24,
            winit::keyboard::KeyCode::F25 => KeyCode::KEY_F25,
            // F26-35
            _ => KeyCode::KEY_INVALID,
        }
    } else {
        return KeyCode::KEY_INVALID;
    }
}
