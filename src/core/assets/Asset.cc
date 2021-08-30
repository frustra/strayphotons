#include "Asset.hh"

#include <murmurhash/MurmurHash3.h>

namespace sp {
    Hash128 Asset::Hash() const {
        Hash128 output;
        MurmurHash3_x86_128(buffer.data(), buffer.size(), 0, output.data());
        return output;
    }
} // namespace sp
