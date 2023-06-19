/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#if RUST_CXX
    #include <rust/cxx.h>
    #include <string>
    #include <unordered_map>

namespace sp::rs {
    void RegisterPrefabScript(rust::String name);
    void RegisterOnTickScript(rust::String name);
    void RegisterOnPhysicsUpdateScript(rust::String name);
} // namespace sp::rs
#endif
