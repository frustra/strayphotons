use std::ops::Add;
use std::sync::Arc;
use std::time::{Duration, Instant};
use cxx::CxxString;
use vulkano::{
    instance::{
        debug::{
            DebugUtilsMessageSeverity, DebugUtilsMessageType, DebugUtilsMessengerCallback,
            DebugUtilsMessengerCreateInfo,
        },
        Instance, InstanceCreateInfo,
    },
    swapchain::Surface,
    VulkanLibrary,
};
use vulkano::{Handle, VulkanObject};
use ::winit::dpi::PhysicalSize;
use ::winit::event::{ElementState, Event, MouseScrollDelta, WindowEvent, DeviceEvent};
use ::winit::keyboard::KeyCode;
use ::winit::platform::scancode::KeyCodeExtScancode;
use ::winit::window::{Window, CursorGrabMode};
use ::winit::{event_loop::EventLoop, window::WindowBuilder};

pub struct WinitContext {
    pub library: Arc<VulkanLibrary>,
    pub instance: Option<Arc<Instance>>,
    pub window: Option<Arc<Window>>,
    pub surface: Option<Arc<Surface>>,
    pub event_loop: Option<EventLoop<()>>,
}

#[cxx::bridge(namespace = "sp")]
mod winit {
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
        include!("sp-rs/include/winit.hh");
        type KeyCode;
        type InputAction;
        type MouseButton;
        type WinitInputHandler;

        unsafe fn InputFrameCallback(ctx: *mut WinitInputHandler) -> bool;
        unsafe fn KeyInputCallback(
            ctx: *mut WinitInputHandler,
            key: KeyCode,
            scancode: i32,
            action: InputAction,
        );
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
        fn create_context(width: i32, height: i32) -> Box<WinitContext>;
        fn destroy_window(context: &mut WinitContext);
        fn destroy_surface(context: &mut WinitContext);
        fn destroy_instance(context: &mut WinitContext);
        fn get_surface_handle(context: &WinitContext) -> u64;
        fn get_instance_handle(context: &WinitContext) -> u64;
        fn get_monitor_modes(context: &WinitContext) -> Vec<MonitorMode>;
        fn set_window_title(context: &WinitContext, title: &CxxString);
        fn set_input_mode(context: &WinitContext, mode: InputMode);
        unsafe fn start_event_loop(context: &mut WinitContext, ctx: *mut WinitInputHandler, max_input_rate: u32);
    }
}

unsafe impl Send for WinitContext {}
unsafe impl Sync for WinitContext {}

fn create_context(width: i32, height: i32) -> Box<WinitContext> {
    let library: Arc<VulkanLibrary> = VulkanLibrary::new().expect("no local Vulkan library/DLL");

    let event_loop = EventLoop::new().unwrap();
    let mut required_extensions: vulkano::instance::InstanceExtensions =
        Surface::required_extensions(&event_loop);

    required_extensions.khr_surface = true;
    required_extensions.khr_get_physical_device_properties2 = true;
    required_extensions.ext_debug_utils = true;

    // TODO: Match these settings with C++ DeviceContext.cc
    let debug_create_info = unsafe {
        DebugUtilsMessengerCreateInfo {
            message_severity: DebugUtilsMessageSeverity::ERROR
                | DebugUtilsMessageSeverity::WARNING,
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
            enabled_layers: vec!["VK_LAYER_KHRONOS_validation".to_string()],
            debug_utils_messengers: vec![debug_create_info],
            ..Default::default()
        },
    )
    .expect("failed to create instance");

    let window: Arc<Window> = Arc::new(
        WindowBuilder::new()
            .with_title("STRAY PHOTONS")
            .with_resizable(false)
            .with_inner_size(PhysicalSize {
                width,
                height,
            })
            .build(&event_loop)
            .expect("Failed to create window"),
    );

    let surface = Surface::from_window(instance.clone(), window.clone()).unwrap();

    println!("Hello, Rust!");
    Box::new(WinitContext {
        library: library.to_owned(),
        instance: Some(instance.to_owned()),
        window: Some(window.to_owned()),
        surface: Some(surface.to_owned()),
        event_loop: Some(event_loop),
    })
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

fn get_monitor_modes(context: &WinitContext) -> Vec<winit::MonitorMode> {
    if let Some(window) = context.window.as_ref() {
        if let Some(monitor) = window.current_monitor().as_ref() {
            monitor.video_modes().map(|mode| {
                let size = mode.size();
                winit::MonitorMode {
                    width: size.width,
                    height: size.height,
                    bit_depth: mode.bit_depth(),
                    refresh_rate_millihertz: mode.refresh_rate_millihertz(),
                }
            }).collect()
        } else {
            vec![]
        }
    } else {
        vec![]
    }
}

fn set_window_title(context: &WinitContext, title: &CxxString) {
    if let Some(window) = context.window.as_ref() {
        window.set_title(title.to_str().unwrap_or("error"));
    }
}

fn set_input_mode(context: &WinitContext, mode: winit::InputMode) {
    if let Some(window) = context.window.as_ref() {
        match mode {
            winit::InputMode::CursorDisabled => {
                window.set_cursor_grab(CursorGrabMode::Locked)
                      .or_else(|_e| window.set_cursor_grab(CursorGrabMode::Confined))
                      .unwrap();
                window.set_cursor_visible(false);
            },
            winit::InputMode::CursorNormal => {
                window.set_cursor_grab(CursorGrabMode::None).unwrap();
                window.set_cursor_visible(true);
            },
            _ => (),
        }
    }
}

fn start_event_loop(context: &mut WinitContext, input_handler: *mut winit::WinitInputHandler, max_input_rate: u32) {
    let event_loop = context.event_loop.take().unwrap();
    event_loop
        .run(|event, _, control_flow| {
            match event {
                Event::DeviceEvent { event, .. } => match event {
                    // TODO: Map unique device_ids to different events/signals
                    DeviceEvent::MouseMotion { delta } => unsafe {
                        winit::MouseMoveCallback(input_handler, delta.0, delta.1);
                    },
                    _ => (),
                },
                Event::WindowEvent { event, .. } => match event {
                    WindowEvent::CloseRequested => {
                        println!("Exit requested by Winit");
                        control_flow.set_exit();
                    }
                    WindowEvent::CursorMoved { position, .. } => unsafe {
                        winit::MousePositionCallback(input_handler, position.x, position.y);
                    },
                    WindowEvent::MouseWheel {
                        delta: MouseScrollDelta::LineDelta(dx, dy),
                        // TODO: Support PixelDelta variant
                        ..
                    } => unsafe {
                        winit::MouseScrollCallback(input_handler, dx as f64, dy as f64);
                    },
                    WindowEvent::MouseInput { state, button, .. } => {
                        let action: winit::InputAction = match state {
                            ElementState::Released => winit::InputAction::RELEASE,
                            ElementState::Pressed => winit::InputAction::PRESS,
                        };
                        let btn: winit::MouseButton = match button {
                            ::winit::event::MouseButton::Left => winit::MouseButton::BUTTON_LEFT,
                            ::winit::event::MouseButton::Right => winit::MouseButton::BUTTON_RIGHT,
                            ::winit::event::MouseButton::Middle => {
                                winit::MouseButton::BUTTON_MIDDLE
                            }
                            ::winit::event::MouseButton::Other(3) => winit::MouseButton::BUTTON_4,
                            ::winit::event::MouseButton::Other(4) => winit::MouseButton::BUTTON_5,
                            ::winit::event::MouseButton::Other(5) => winit::MouseButton::BUTTON_6,
                            ::winit::event::MouseButton::Other(6) => winit::MouseButton::BUTTON_7,
                            ::winit::event::MouseButton::Other(7) => winit::MouseButton::BUTTON_8,
                            _ => winit::MouseButton::BUTTON_LEFT,
                        };
                        unsafe {
                            winit::MouseButtonCallback(input_handler, btn, action);
                        }
                    }
                    WindowEvent::KeyboardInput { event, .. } => {
                        let key: winit::KeyCode = into_keycode(event.physical_key);
                        let scancode: u32 = event.physical_key.to_scancode().unwrap_or(0u32);
                        let mut action: winit::InputAction = match event.state {
                            ElementState::Released => winit::InputAction::RELEASE,
                            ElementState::Pressed => winit::InputAction::PRESS,
                        };
                        if event.repeat && action == winit::InputAction::PRESS {
                            action = winit::InputAction::REPEAT;
                        }
                        unsafe {
                            winit::KeyInputCallback(input_handler, key, scancode as i32, action);
                        }

                        if let Some(text) = event.text {
                            for ch in text.chars() {
                                unsafe {
                                    winit::CharInputCallback(input_handler, ch as u32);
                                }
                            }
                        }
                    },
                    _ => (),
                },
                Event::AboutToWait => {
                    let should_close: bool;
                    unsafe {
                        should_close = !winit::InputFrameCallback(input_handler);
                    }
                    if should_close {
                        control_flow.set_exit();
                    } else if max_input_rate > 0 {
                        control_flow.set_wait_until(Instant::now().add(Duration::from_secs(1) / max_input_rate));
                    } else {
                        control_flow.set_wait();
                    }
                }
                _ => (),
            }
        })
        .unwrap();
}

fn into_keycode(winit_key: KeyCode) -> winit::KeyCode {
    match winit_key {
        KeyCode::Backquote => winit::KeyCode::KEY_BACKTICK,
        KeyCode::Backslash => winit::KeyCode::KEY_BACKSLASH,
        KeyCode::BracketLeft => winit::KeyCode::KEY_LEFT_BRACKET,
        KeyCode::BracketRight => winit::KeyCode::KEY_RIGHT_BRACKET,
        KeyCode::Comma => winit::KeyCode::KEY_COMMA,
        KeyCode::Digit0 => winit::KeyCode::KEY_0,
        KeyCode::Digit1 => winit::KeyCode::KEY_1,
        KeyCode::Digit2 => winit::KeyCode::KEY_2,
        KeyCode::Digit3 => winit::KeyCode::KEY_3,
        KeyCode::Digit4 => winit::KeyCode::KEY_4,
        KeyCode::Digit5 => winit::KeyCode::KEY_5,
        KeyCode::Digit6 => winit::KeyCode::KEY_6,
        KeyCode::Digit7 => winit::KeyCode::KEY_7,
        KeyCode::Digit8 => winit::KeyCode::KEY_8,
        KeyCode::Digit9 => winit::KeyCode::KEY_9,
        KeyCode::Equal => winit::KeyCode::KEY_EQUALS,
        // IntlBackslash
        // IntlRo
        // IntlYen
        KeyCode::KeyA => winit::KeyCode::KEY_A,
        KeyCode::KeyB => winit::KeyCode::KEY_B,
        KeyCode::KeyC => winit::KeyCode::KEY_C,
        KeyCode::KeyD => winit::KeyCode::KEY_D,
        KeyCode::KeyE => winit::KeyCode::KEY_E,
        KeyCode::KeyF => winit::KeyCode::KEY_F,
        KeyCode::KeyG => winit::KeyCode::KEY_G,
        KeyCode::KeyH => winit::KeyCode::KEY_H,
        KeyCode::KeyI => winit::KeyCode::KEY_I,
        KeyCode::KeyJ => winit::KeyCode::KEY_J,
        KeyCode::KeyK => winit::KeyCode::KEY_K,
        KeyCode::KeyL => winit::KeyCode::KEY_L,
        KeyCode::KeyM => winit::KeyCode::KEY_M,
        KeyCode::KeyN => winit::KeyCode::KEY_N,
        KeyCode::KeyO => winit::KeyCode::KEY_O,
        KeyCode::KeyP => winit::KeyCode::KEY_P,
        KeyCode::KeyQ => winit::KeyCode::KEY_Q,
        KeyCode::KeyR => winit::KeyCode::KEY_R,
        KeyCode::KeyS => winit::KeyCode::KEY_S,
        KeyCode::KeyT => winit::KeyCode::KEY_T,
        KeyCode::KeyU => winit::KeyCode::KEY_U,
        KeyCode::KeyV => winit::KeyCode::KEY_V,
        KeyCode::KeyW => winit::KeyCode::KEY_W,
        KeyCode::KeyX => winit::KeyCode::KEY_X,
        KeyCode::KeyY => winit::KeyCode::KEY_Y,
        KeyCode::KeyZ => winit::KeyCode::KEY_Z,
        KeyCode::Minus => winit::KeyCode::KEY_MINUS,
        KeyCode::Period => winit::KeyCode::KEY_PERIOD,
        KeyCode::Quote => winit::KeyCode::KEY_APOSTROPHE,
        KeyCode::Semicolon => winit::KeyCode::KEY_SEMICOLON,
        KeyCode::Slash => winit::KeyCode::KEY_SLASH,
        KeyCode::AltLeft => winit::KeyCode::KEY_LEFT_ALT,
        KeyCode::AltRight => winit::KeyCode::KEY_RIGHT_ALT,
        KeyCode::Backspace => winit::KeyCode::KEY_BACKSPACE,
        KeyCode::CapsLock => winit::KeyCode::KEY_CAPS_LOCK,
        KeyCode::ContextMenu => winit::KeyCode::KEY_CONTEXT_MENU,
        KeyCode::ControlLeft => winit::KeyCode::KEY_LEFT_CONTROL,
        KeyCode::ControlRight => winit::KeyCode::KEY_RIGHT_CONTROL,
        KeyCode::Enter => winit::KeyCode::KEY_ENTER,
        KeyCode::SuperLeft => winit::KeyCode::KEY_LEFT_SUPER,
        KeyCode::SuperRight => winit::KeyCode::KEY_RIGHT_SUPER,
        KeyCode::ShiftLeft => winit::KeyCode::KEY_LEFT_SHIFT,
        KeyCode::ShiftRight => winit::KeyCode::KEY_RIGHT_SHIFT,
        KeyCode::Space => winit::KeyCode::KEY_SPACE,
        KeyCode::Tab => winit::KeyCode::KEY_TAB,
        // Convert
        // KanaMode
        // Lang1
        // Lang2
        // Lang3
        // Lang4
        // NonConvert
        KeyCode::Delete => winit::KeyCode::KEY_DELETE,
        KeyCode::End => winit::KeyCode::KEY_END,
        // Help
        KeyCode::Home => winit::KeyCode::KEY_HOME,
        KeyCode::Insert => winit::KeyCode::KEY_INSERT,
        KeyCode::PageDown => winit::KeyCode::KEY_PAGE_DOWN,
        KeyCode::PageUp => winit::KeyCode::KEY_PAGE_UP,
        KeyCode::ArrowDown => winit::KeyCode::KEY_DOWN_ARROW,
        KeyCode::ArrowLeft => winit::KeyCode::KEY_LEFT_ARROW,
        KeyCode::ArrowRight => winit::KeyCode::KEY_RIGHT_ARROW,
        KeyCode::ArrowUp => winit::KeyCode::KEY_UP_ARROW,
        KeyCode::NumLock => winit::KeyCode::KEY_NUM_LOCK,
        KeyCode::Numpad0 => winit::KeyCode::KEY_0_NUMPAD,
        KeyCode::Numpad1 => winit::KeyCode::KEY_1_NUMPAD,
        KeyCode::Numpad2 => winit::KeyCode::KEY_2_NUMPAD,
        KeyCode::Numpad3 => winit::KeyCode::KEY_3_NUMPAD,
        KeyCode::Numpad4 => winit::KeyCode::KEY_4_NUMPAD,
        KeyCode::Numpad5 => winit::KeyCode::KEY_5_NUMPAD,
        KeyCode::Numpad6 => winit::KeyCode::KEY_6_NUMPAD,
        KeyCode::Numpad7 => winit::KeyCode::KEY_7_NUMPAD,
        KeyCode::Numpad8 => winit::KeyCode::KEY_8_NUMPAD,
        KeyCode::Numpad9 => winit::KeyCode::KEY_9_NUMPAD,
        KeyCode::NumpadAdd => winit::KeyCode::KEY_PLUS_NUMPAD,
        // NumpadBackspace
        // NumpadClear
        // NumpadClearEntry
        // NumpadComma
        KeyCode::NumpadDecimal => winit::KeyCode::KEY_DECIMAL_NUMPAD,
        KeyCode::NumpadDivide => winit::KeyCode::KEY_DIVIDE_NUMPAD,
        KeyCode::NumpadEnter => winit::KeyCode::KEY_ENTER_NUMPAD,
        KeyCode::NumpadEqual => winit::KeyCode::KEY_EQUALS_NUMPAD,
        // NumpadHash
        // NumpadMemoryAdd
        // NumpadMemoryClear
        // NumpadMemoryRecall
        // NumpadMemoryStore
        // NumpadMemorySubtract
        KeyCode::NumpadMultiply => winit::KeyCode::KEY_MULTIPLY_NUMPAD,
        // NumpadParenLeft
        // NumpadParenRight
        // NumpadStar
        KeyCode::NumpadSubtract => winit::KeyCode::KEY_MINUS_NUMPAD,
        KeyCode::Escape => winit::KeyCode::KEY_ESCAPE,
        // Fn
        // FnLock
        KeyCode::PrintScreen => winit::KeyCode::KEY_PRINT_SCREEN,
        KeyCode::ScrollLock => winit::KeyCode::KEY_SCROLL_LOCK,
        KeyCode::Pause => winit::KeyCode::KEY_PAUSE,
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
        KeyCode::F1 => winit::KeyCode::KEY_F1,
        KeyCode::F2 => winit::KeyCode::KEY_F2,
        KeyCode::F3 => winit::KeyCode::KEY_F3,
        KeyCode::F4 => winit::KeyCode::KEY_F4,
        KeyCode::F5 => winit::KeyCode::KEY_F5,
        KeyCode::F6 => winit::KeyCode::KEY_F6,
        KeyCode::F7 => winit::KeyCode::KEY_F7,
        KeyCode::F8 => winit::KeyCode::KEY_F8,
        KeyCode::F9 => winit::KeyCode::KEY_F9,
        KeyCode::F10 => winit::KeyCode::KEY_F10,
        KeyCode::F11 => winit::KeyCode::KEY_F11,
        KeyCode::F12 => winit::KeyCode::KEY_F12,
        KeyCode::F13 => winit::KeyCode::KEY_F13,
        KeyCode::F14 => winit::KeyCode::KEY_F14,
        KeyCode::F15 => winit::KeyCode::KEY_F15,
        KeyCode::F16 => winit::KeyCode::KEY_F16,
        KeyCode::F17 => winit::KeyCode::KEY_F17,
        KeyCode::F18 => winit::KeyCode::KEY_F18,
        KeyCode::F19 => winit::KeyCode::KEY_F19,
        KeyCode::F20 => winit::KeyCode::KEY_F20,
        KeyCode::F21 => winit::KeyCode::KEY_F21,
        KeyCode::F22 => winit::KeyCode::KEY_F22,
        KeyCode::F23 => winit::KeyCode::KEY_F23,
        KeyCode::F24 => winit::KeyCode::KEY_F24,
        KeyCode::F25 => winit::KeyCode::KEY_F25,
        // F26-35
        _ => winit::KeyCode::KEY_INVALID,
    }
}
