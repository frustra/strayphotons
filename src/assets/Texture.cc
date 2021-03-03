#include "Texture.hh"

#include "assets/Asset.hh"
#include "core/Logging.hh"

#include <functional>
#include <algorithm>
#include <stb_image.h>

namespace sp {

    Texture::Texture(StbImagePtr i, int w, int h, int c) :
        image(i),
        width(w),
        height(h),
        components(c) {

    }

    TexturePtr Texture::LoadFromAsset(std::shared_ptr<Asset> asset) {
        Assert(asset != nullptr, "loading asset from null asset");

        int w, h, comp;
        uint8_t* data = stbi_load_from_memory(asset->Buffer(), asset->Size(), &w, &h, &comp, 0);

        Assert(data, "unknown image format");
        Assert(w > 0 && h > 0, "unknown image format");
        
        // Wrap the image data in a smart pointer that will automatically free it when the 
        // pointer goes out of scope.
        StbImagePtr image(data, [](uint8_t* ptr) { 
            Logf("Texture destructor"); 
            stbi_image_free((void*) ptr); 
        });

        return TexturePtr(new Texture(image, w, h, comp));
    }
} // namespace sp
