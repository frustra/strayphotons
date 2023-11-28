/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#[cxx::bridge(namespace = "sp::api")]
mod ctx {
    unsafe extern "C++" {
        include!("sp-rs/include/api.hh");

        type Api;

        fn new_api() -> UniquePtr<Api>;
    }
}
