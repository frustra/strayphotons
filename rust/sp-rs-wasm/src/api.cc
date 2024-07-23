/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "sp-rs-wasm/include/api.hh"

#include "sp-rs-wasm/src/api.rs.h"

namespace sp {
    namespace api {
        Api::Api() {}

        std::unique_ptr<Api> new_api() {
            return std::make_unique<Api>();
        }
    } // namespace api
} // namespace sp
