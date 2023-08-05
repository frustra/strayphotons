use crate::error::StrayPhotonsError;
use crate::game::{game_destroy, game_init, game_start};

use std::error::Error;

#[derive(Default)]
pub struct StrayPhotons(u64);

impl StrayPhotons {
    pub fn new() -> Self {
        StrayPhotons::default()
    }

    pub unsafe fn start(mut self) -> Result<(), Box<dyn Error>> {
        let mut input = ["sp"];

        self.0 = game_init(1, input.as_mut_ptr() as *mut *mut i8);
        let status = game_start(self.0);
        match status {
            0 => Ok(()),
            e => Err(Box::new(StrayPhotonsError::new(e))),
        }
    }
}

impl Drop for StrayPhotons {
    fn drop(&mut self) {
        unsafe {
            game_destroy(self.0);
        }
    }
}
