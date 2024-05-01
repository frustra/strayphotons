#[allow(dead_code)]
#[allow(non_camel_case_types)]
#[allow(non_upper_case_globals)]
pub mod game;

mod sp;
pub use sp::StrayPhotons;
pub use sp::Game;

#[cfg(test)]
mod tests {
    use crate::sp::{StrayPhotons, Game};
    use std::error::Error;

    #[test]
    fn test_sp() -> Result<(), Box<dyn Error>> {
        unsafe {
            let mut sp = StrayPhotons::init(&vec![]);
            let mut game: Game = sp.game();
            game.start();
            game.trigger_exit();
            game.wait_for_exit();
        }
        Ok(())
    }
}
