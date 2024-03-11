/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "assets/Image.hh"
#include "common/Common.hh"

#include <memory>

namespace sp {
    class GpuTexture {
    public:
        virtual uintptr_t GetHandle() const = 0;
        virtual int GetWidth() const = 0;
        virtual int GetHeight() const = 0;
    };

    /**
     * @brief A generic class that stores a reference to a texture. The referenced texture may be resident on
     *        the CPU, GPU, or both depending on the context.
     *        This class is separate from any particular graphics pipeline, and must support headless environments.
     */
    class Texture : public NonCopyable {
    public:
        Texture(std::shared_ptr<Image> source) : cpu(source) {}
        Texture(std::shared_ptr<GpuTexture> source) : gpu(source) {}

    private:
        std::shared_ptr<Image> cpu;
        std::shared_ptr<GpuTexture> gpu;
    };
} // namespace sp
