/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"

#include <functional>
#include <memory>
#include <span>

namespace sp {
    class Asset;

    /**
     * An Image asset is a texture that has been loaded into CPU memory.
     */
    class Image : public NonCopyable {
    public:
        Image(std::shared_ptr<const Asset> asset);
        Image(std::span<const unsigned char> bufferView);
        Image(const uint8_t *data, size_t dataSize, uint32_t width, uint32_t height, uint32_t components);

        int GetWidth() const {
            return width;
        }

        int GetHeight() const {
            return height;
        }

        int GetComponents() const {
            return components;
        }

        std::shared_ptr<const uint8_t> GetImage() const {
            return image;
        }

        size_t ByteSize() const {
            // stbi_load_from_memory returns 8 bits per channel
            return width * height * components;
        }

    private:
        int width, height, components;
        std::shared_ptr<const uint8_t> image;
    };
} // namespace sp
