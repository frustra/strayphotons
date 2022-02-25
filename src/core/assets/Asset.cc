#include "Asset.hh"

#include <murmurhash/MurmurHash3.h>

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
