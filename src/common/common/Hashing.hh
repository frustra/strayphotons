/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"

#include <cstring>
#include <robin_hood.h>

namespace sp {
    inline Hash64 Hash128To64(Hash128 input) {
        return input[0] + 0x9e3779b9 + (input[1] << 6) + (input[1] >> 2);
    }

    template<typename T, typename U>
    void hash_combine(T &seed, const U &val) {
        seed ^= std::hash<U>()(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
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

        Hash128 Hash_128() const {
            Hash128 hash = {0, 0};
            for (size_t i = 0; i < std::size(words); i++) {
                // TODO: benchmark vs murmur3 for varying sized inputs
                hash_combine(hash[i % 2], words[i]);
            }
            return hash;
        }

        struct Hasher {
            uint64 operator()(const HashKey<T> &key) const {
                return key.Hash();
            }
        };
    };

    struct StringHash {
        using is_transparent = void;

        std::size_t operator()(const std::string &key) const {
            return robin_hood::hash_bytes(key.c_str(), key.size());
        }
        std::size_t operator()(std::string_view key) const {
            return robin_hood::hash_bytes(key.data(), key.size());
        }
        std::size_t operator()(const char *key) const {
            return robin_hood::hash_bytes(key, std::strlen(key));
        }
    };

    struct StringEqual {
        using is_transparent = void;

        bool operator()(const std::string_view &lhs, const std::string &rhs) const {
            const std::string_view view = rhs;
            return lhs == view;
        }
        bool operator()(const std::string &lhs, const std::string_view &rhs) const {
            const std::string_view view = lhs;
            return view == rhs;
        }
        bool operator()(const char *lhs, const std::string &rhs) const {
            return std::strcmp(lhs, rhs.c_str()) == 0;
        }
        bool operator()(const std::string &lhs, const std::string &rhs) const {
            return lhs == rhs;
        }
    };
} // namespace sp
