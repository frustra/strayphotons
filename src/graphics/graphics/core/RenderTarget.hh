#pragma once

#include <memory>

namespace sp {
    class GpuTexture;

    class RenderTarget {
    public:
        virtual GpuTexture *GetTexture() = 0;
    };
} // namespace sp
