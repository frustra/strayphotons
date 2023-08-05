mod error;
#[allow(dead_code)]
#[allow(non_snake_case)]
#[allow(non_camel_case_types)]
#[allow(non_upper_case_globals)]
mod game;

mod sp;
pub use sp::StrayPhotons;

#[cfg(test)]
mod tests {
    use crate::sp::StrayPhotons;
    use std::error::Error;

    #[test]
    fn test_sp() -> Result<(), Box<dyn Error>> {
        let sp = StrayPhotons::new();
        unsafe {
            sp.start()?;
        }
        Ok(())
    }
}
