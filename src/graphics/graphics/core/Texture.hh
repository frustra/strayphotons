#pragma once

#include "core/Common.hh"

#include <memory>

namespace sp {
    class Image;

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

    private:
        std::shared_ptr<Image> cpu;
        std::shared_ptr<GpuTexture> gpu;
    };
} // namespace sp
