#pragma once

#include "ecs/Components.hh"

namespace sp {
    class Asset;
}

namespace ecs {
    class Sound {
    public:
        shared_ptr<const sp::Asset> file; // TODO: should make the asset system unpack the audio file
    };

    static Component<Sound> ComponentSound("sound");

    template<>
    bool Component<Sound>::Load(sp::Scene *scene, Sound &dst, const picojson::value &src);
} // namespace ecs
