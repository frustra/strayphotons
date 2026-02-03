/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Asset.hh"

#include "common/Logging.hh"

#include <MurmurHash3.h>

namespace sp {
    Hash128 Asset::Hash() const {
        if (hash) {
            return *hash;
        } else {
            Hash128 output;
            Assert(buffer.size() <= INT_MAX, "Buffer size overflows int");
            MurmurHash3_x86_128(buffer.data(), (int)buffer.size(), 0, output.data());

            // This isn't really safe, but this cache isn't too important anyway.
            const_cast<Asset *>(this)->hash = output;
            return output;
        }
    }
} // namespace sp
