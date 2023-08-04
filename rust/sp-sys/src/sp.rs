use crate::game::{game_init, game_start, game_destroy};
use crate::error::StrayPhotonsError;

use std::error::Error;
use std::ptr;
use std::ffi::CStr;
use std::ffi::CString;

#[derive(Default)]
pub struct StrayPhotons (u64);

impl StrayPhotons {
    pub fn new() -> Self {
        StrayPhotons::default()
    }

    pub unsafe fn start(mut self) -> Result<(), Box<dyn Error>> {
        self.0 = game_init(0, ptr::null::<i8>() as *mut *mut i8);
        let status = game_start(self.0);
        match status {
            0 => Ok(()),
            e => Err(Box::new(StrayPhotonsError::new(e))),
        }
    }
}

impl Drop for StrayPhotons {
    fn drop(&mut self) {
        unsafe { game_destroy(self.0); }
    }
}
