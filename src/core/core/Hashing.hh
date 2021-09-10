#pragma once

#include "Common.hh"

#include <cstring>

namespace sp {
    inline Hash64 Hash128To64(Hash128 input) {
        return input[0] + 0x9e3779b9 + (input[1] << 6) + (input[1] >> 2);
    }

    template<typename T>
    void hash_combine(uint64 &seed, const T &val) {
        // std::hash returns size_t, which is compatible with uint64 on 64 bit systems
        // We always want 64 bit hashes, so this code explicitly uses uint64 to cause
        // a warning on 32 bit builds.
        seed ^= std::hash<T>()(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    template<typename T>
    union HashKey {
        HashKey() {}
        HashKey(const T &input) : input(input) {}

        T input;
        uint64 words[(sizeof(input) + sizeof(uint64) - 1) / sizeof(uint64)] = {0};

        bool operator==(const HashKey<T> &other) const {
            return std::memcmp(words, other.words, sizeof(words)) == 0;
        }

        Hash64 Hash() const {
            uint64 hash = 0;
            for (size_t i = 0; i < std::size(words); i++) {
                // TODO: benchmark vs murmur3 for varying sized inputs
                hash_combine(hash, words[i]);
            }
            return hash;
        }

        struct Hasher {
            uint64 operator()(const HashKey<T> &key) const {
                return key.Hash();
            }
        };
    };

} // namespace sp
