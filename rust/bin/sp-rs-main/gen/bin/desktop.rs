fn main() {
    #[cfg(not(any(target_os = "android", target_os = "ios")))]
    sp_rs_main::start_app();
}
