use crate::game::*;

use std::ffi::CString;
use std::ffi::c_void;

pub struct StrayPhotons(*mut c_void);
#[derive(Debug, Copy, Clone)]
pub struct Game(*mut c_void);
#[derive(Debug, Copy, Clone)]
pub struct GraphicsContext(*mut c_void);
#[derive(Debug, Copy, Clone)]
pub struct CVar(*mut c_void);
#[derive(Debug, Copy, Clone)]
pub struct Entity(u64);

impl StrayPhotons {
    pub unsafe fn init(args: &[&str]) -> Self {
        let mut c_ptrs: Vec<*mut i8> = args.iter().map(|s| CString::new(*s).unwrap().into_raw()).collect();
        c_ptrs.insert(0, CString::new("sp-rs").unwrap().into_raw());

        let sp = Self {
            0: sp_game_init(c_ptrs.len() as i32, c_ptrs.as_mut_ptr()),
        };
        for ptr in c_ptrs {
            let _ = CString::from_raw(ptr);
        }
        sp
    }

    pub fn game(&mut self) -> Game {
        Game { 0: self.0 }
    }
}

impl Drop for StrayPhotons {
    fn drop(&mut self) {
        if self.0 != std::ptr::null_mut() {
            unsafe {
                sp_game_destroy(self.0);
            }
        }
    }
}

impl From<Game> for usize {
    fn from(game: Game) -> usize {
        game.0 as usize
    }
}

impl From<usize> for Game {
    fn from(game_handle: usize) -> Self {
        Game { 0: game_handle as *mut c_void }
    }
}

impl Game {
    pub unsafe fn get_cli_flag(&mut self, arg_name: &str) -> bool {
        let c_str = CString::new(arg_name).unwrap();
        sp_game_get_cli_flag(self.0, c_str.as_ptr())
    }

    pub unsafe fn start(&mut self) -> i32 {
        sp_game_start(self.0)
    }

    pub unsafe fn trigger_exit(&mut self) {
        sp_game_trigger_exit(self.0);
    }

    pub unsafe fn is_exit_triggered(&mut self) -> bool {
        sp_game_is_exit_triggered(self.0)
    }

    pub unsafe fn wait_for_exit_trigger(&mut self) -> i32 {
        sp_game_wait_for_exit_trigger(self.0)
    }

    pub unsafe fn get_exit_code(&mut self) -> i32 {
        sp_game_get_exit_code(self.0)
    }

    pub unsafe fn get_cvar(name: &str) -> CVar {
        let c_str = CString::new(name).unwrap();
        CVar {
            0: sp_get_cvar(c_str.as_ptr()),
        }
    }

    pub unsafe fn register_cfunc_u32(name: &str, description: &str, cfunc_callback: extern "C" fn(u32)) -> CVar {
        let name_str = CString::new(name).unwrap();
        let desc_str = CString::new(description).unwrap();
        CVar {
            0: sp_register_cfunc_uint32(name_str.as_ptr(), desc_str.as_ptr(), Some(cfunc_callback)),
        }
    }

    pub unsafe fn get_graphics_context(&mut self) -> GraphicsContext {
        GraphicsContext {
            0: sp_game_get_graphics_context(self.0),
        }
    }

    pub unsafe fn new_input_device(&mut self, name: &str) -> Entity {
        let name_str = CString::new(name).unwrap();
        Entity {
            0: sp_new_input_device(self.0, name_str.as_ptr()),
        }
    }

    pub unsafe fn send_input_bool(&mut self, input_device: Entity, event_name: &str, value: bool) {
        let name_str = CString::new(event_name).unwrap();
        sp_send_input_bool(self.0, input_device.0, name_str.as_ptr(), if value { 1 } else { 0 });
    }

    pub unsafe fn send_input_str(&mut self, input_device: Entity, event_name: &str, value: &str) {
        let name_str = CString::new(event_name).unwrap();
        let value_str = CString::new(value).unwrap();
        sp_send_input_str(self.0, input_device.0, name_str.as_ptr(), value_str.as_ptr());
    }

    pub unsafe fn send_input_int(&mut self, input_device: Entity, event_name: &str, value: i32) {
        let name_str = CString::new(event_name).unwrap();
        sp_send_input_int(self.0, input_device.0, name_str.as_ptr(), value);
    }

    pub unsafe fn send_input_uint(&mut self, input_device: Entity, event_name: &str, value: u32) {
        let name_str = CString::new(event_name).unwrap();
        sp_send_input_uint(self.0, input_device.0, name_str.as_ptr(), value);
    }

    pub unsafe fn send_input_vec2(&mut self, input_device: Entity, event_name: &str, value: (f32, f32)) {
        let name_str = CString::new(event_name).unwrap();
        sp_send_input_vec2(self.0, input_device.0, name_str.as_ptr(), value.0, value.1);
    }
}

impl CVar {
    pub unsafe fn get_bool(&mut self) -> bool {
        sp_cvar_get_bool(self.0)
    }
    pub unsafe fn set_bool(&mut self, value: bool) {
        sp_cvar_set_bool(self.0, value)
    }

    pub unsafe fn get_f32(&mut self) -> f32 {
        sp_cvar_get_float(self.0)
    }
    pub unsafe fn set_f32(&mut self, value: f32) {
        sp_cvar_set_float(self.0, value)
    }

    pub unsafe fn get_u32(&mut self) -> u32 {
        sp_cvar_get_uint32(self.0)
    }
    pub unsafe fn set_u32(&mut self, value: u32) {
        sp_cvar_set_uint32(self.0, value)
    }

    pub unsafe fn get_ivec2(&mut self) -> (i32, i32) {
        let mut value: (i32, i32) = (0, 0);
        sp_cvar_get_ivec2(self.0, &mut value.0, &mut value.1);
        value
    }
    pub unsafe fn set_ivec2(&mut self, value: (i32, i32)) {
        sp_cvar_set_ivec2(self.0, value.0, value.1)
    }

    pub unsafe fn get_vec2(&mut self) -> (f32, f32) {
        let mut value: (f32, f32) = (0.0, 0.0);
        sp_cvar_get_vec2(self.0, &mut value.0, &mut value.1);
        value
    }
    pub unsafe fn set_vec2(&mut self, value: (f32, f32)) {
        sp_cvar_set_vec2(self.0, value.0, value.1)
    }

    pub unsafe fn unregister_cfunc(&mut self) {
        sp_unregister_cfunc(self.0)
    }
}

impl From<GraphicsContext> for usize {
    fn from(graphics: GraphicsContext) -> usize {
        graphics.0 as usize
    }
}

impl From<usize> for GraphicsContext {
    fn from(graphics_handle: usize) -> Self {
        GraphicsContext { 0: graphics_handle as *mut c_void }
    }
}

impl GraphicsContext {
    pub unsafe fn set_vulkan_instance(&mut self, vk_instance: usize, destroy_callback: unsafe extern "C" fn(*mut sp_graphics_ctx_t, VkInstance)) {
        sp_graphics_set_vulkan_instance(self.0, vk_instance as *mut VkInstance_T, Some(destroy_callback));
    }

    pub unsafe fn set_vulkan_surface(&mut self, vk_surface: usize, destroy_callback: unsafe extern "C" fn(*mut sp_graphics_ctx_t, VkSurfaceKHR)) {
        sp_graphics_set_vulkan_surface(self.0, vk_surface as *mut VkSurfaceKHR_T, Some(destroy_callback));
    }

    pub unsafe fn set_winit_context(&mut self, winit_context: *mut sp_winit_ctx_t, destroy_callback: unsafe extern "C" fn(*mut sp_winit_ctx_t)) {
        sp_graphics_set_winit_context(self.0, winit_context, Some(destroy_callback));
    }

    pub unsafe fn get_winit_context(&mut self) -> *mut sp_winit_ctx_t {
        sp_graphics_get_winit_context(self.0)
    }

    pub unsafe fn set_window_handlers(&mut self, handlers: &sp_window_handlers_t) {
        sp_graphics_set_window_handlers(self.0, &*handlers);
    }

    pub unsafe fn handle_input_frame(&mut self) -> bool {
        sp_graphics_handle_input_frame(self.0)
    }

    pub unsafe fn step_thread(&mut self, count: u32) {
        sp_graphics_step_thread(self.0, count);
    }
}
