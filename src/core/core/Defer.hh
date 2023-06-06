/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

namespace sp {
    template<typename Fn>
    struct Defer {
        Defer(Fn &&fn) : fn(std::move(fn)) {}
        ~Defer() {
            fn();
        }
        Fn fn;
    };
} // namespace sp
