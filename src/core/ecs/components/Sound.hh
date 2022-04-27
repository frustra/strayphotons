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
        bool loop = false, playOnLoad = false;
    };

    struct Sounds {
        vector<Sound> sounds;
    };

    static Component<Sounds> ComponentSound("sound");

    template<>
    bool Component<Sounds>::Load(const EntityScope &scope, Sounds &dst, const picojson::value &src);
} // namespace ecs
