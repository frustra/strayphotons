#pragma once

#include "assets/Async.hh"
#include "ecs/Components.hh"

namespace sp {
    class Asset;
}

namespace ecs {
    class Sound {
    public:
        enum class Type {
            Object,
            Stereo,
            Ambisonic,
        } type = Type::Object;

        sp::AsyncPtr<sp::Asset> file; // TODO: should make the asset system unpack the audio file
        float volume = 1.0f;
    };

    static Component<Sound> ComponentSound("sound");

    template<>
    bool Component<Sound>::Load(const EntityScope &scope, Sound &dst, const picojson::value &src);
} // namespace ecs
