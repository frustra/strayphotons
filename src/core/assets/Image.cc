/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Image.hh"

#include "assets/Asset.hh"
#include "common/Common.hh"
#include "common/Tracing.hh"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace sp {
    Image::Image(std::shared_ptr<const Asset> asset) {
        Assert(asset, "Loading Image from null asset");

        ZoneScopedN("LoadImageFromAsset");
        ZoneStr(asset->path.string());

        Assert(asset->BufferSize() <= INT_MAX, "Buffer size overflows int");
        uint8_t *data = stbi_load_from_memory(asset->BufferPtr(),
            (int)asset->BufferSize(),
            &this->width,
            &this->height,
            &this->components,
            0);

        Assert(data, "unknown image format");
        Assert(this->width > 0 && this->height > 0, "unknown image format");

        // Wrap the image data in a smart pointer that will automatically free it when the image goes away.
        this->image = std::shared_ptr<uint8_t>(data, [](uint8_t *ptr) {
            stbi_image_free((void *)ptr);
        });
    }

    Image::Image(std::span<const unsigned char> bufferView) {
        Assert(!bufferView.empty(), "Loading Image from empty buffer view");

        ZoneScopedN("LoadImageFromBuffer");

        Assert(bufferView.size() <= INT_MAX, "Buffer size overflows int");
        uint8_t *data = stbi_load_from_memory(bufferView.data(),
            (int)bufferView.size(),
            &this->width,
            &this->height,
            &this->components,
            4);

        Assert(data, "unknown image format");
        Assert(this->width > 0 && this->height > 0, "unknown image format");

        // Wrap the image data in a smart pointer that will automatically free it when the image goes away.
        this->image = std::shared_ptr<uint8_t>(data, [](uint8_t *ptr) {
            stbi_image_free((void *)ptr);
        });
    }
} // namespace sp
