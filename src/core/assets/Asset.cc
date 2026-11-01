/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Asset.hh"

#include "strayphotons/Logging.hh"

#include <MurmurHash3.h>

namespace sp {
    Hash128 Asset::Hash() const {
        if (hash) {
            return *hash;
        } else {
            Hash128 output;
            Assert(buffer.size() <= INT_MAX, "Buffer size overflows int");
            MurmurHash3_x86_128(buffer.data(), (int)buffer.size(), 0, output.data());

            hash = output;
            return output;
        }
    }
} // namespace sp
