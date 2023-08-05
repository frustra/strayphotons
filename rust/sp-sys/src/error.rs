use std::error::Error;
use std::fmt;

#[derive(Debug)]
pub struct StrayPhotonsError {
    details: String,
}

impl StrayPhotonsError {
    pub fn new(err: i32) -> StrayPhotonsError {
        StrayPhotonsError {
            details: err.to_string(),
        }
    }
}

impl fmt::Display for StrayPhotonsError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.details)
    }
}

impl Error for StrayPhotonsError {
    fn description(&self) -> &str {
        &self.details
    }
}
