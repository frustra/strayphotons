#include "Asset.hh"

#include <murmurhash/MurmurHash3.h>

namespace sp {
    Hash128 Asset::Hash() const {
        Hash128 output;
        Assert(buffer.size() <= INT_MAX, "Buffer size overflows int");
        MurmurHash3_x86_128(buffer.data(), (int)buffer.size(), 0, output.data());
        return output;
    }
} // namespace sp
