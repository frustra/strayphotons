#include "Image.hh"

#include "assets/Asset.hh"
#include "core/Common.hh"

#include <stb_image.h>

namespace sp {
    Image::Image(std::shared_ptr<const Asset> asset) {
        Assert(asset, "Loading Image from null asset");

        ZoneScopedN("LoadImage");
        ZoneStr(asset->path);

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
} // namespace sp
