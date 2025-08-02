/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"

#include <atomic>
#include <filesystem>
#include <optional>
#include <vector>

namespace sp {
    static std::string parseFileExtension(std::string_view path) {
        auto extension = std::filesystem::path(path).extension().string();
        if (extension.length() > 0 && extension[0] == '.') {
            extension = extension.substr(1);
        }
        return to_lower(extension);
    }

    class Asset : public NonCopyable {
    public:
        Asset(std::string_view path = "") : path(path), extension(parseFileExtension(path)) {}

        std::string String() const {
            return std::string((char *)buffer.data(), buffer.size());
        }

        const std::vector<uint8_t> &Buffer() const {
            return buffer;
        }

        const uint8_t *BufferPtr() const {
            return buffer.data();
        }

        const size_t BufferSize() const {
            return buffer.size();
        }

        Hash128 Hash() const;

        const std::filesystem::path path;
        const std::string extension;

    private:
        std::vector<uint8_t> buffer;
        std::optional<Hash128> hash;

        friend class AssetManager;
    };
} // namespace sp
