#pragma once

#include "assets/Async.hh"
#include "ecs/Components.hh"
#include "ecs/components/Events.hh"

namespace sp {
    class Asset;
}

namespace ecs {
    class Sound {
    public:
        // Set these fields before adding the Sound to the scene
        enum class Type {
            Object,
            Stereo,
            Ambisonic,
        } type = Type::Object;

        sp::AsyncPtr<sp::Asset> file; // TODO: should make the asset system unpack the audio file
        bool loop = false, playOnLoad = false;

        // Update these fields at any point
        float volume = 1.0f;
    };

    struct Sounds {
        vector<Sound> sounds;
        EventQueueRef eventQueue;

        // Update these fields at any point
        float occlusion = 0.0f, occlusionWeight = 1.0f;
    };

    static StructMetadata MetadataSounds(typeid(Sounds));
    static Component<Sounds> ComponentSound("sound", MetadataSounds);

    template<>
    bool Component<Sounds>::Load(const EntityScope &scope, Sounds &dst, const picojson::value &src);
    template<>
    void Component<Sounds>::Apply(const Sounds &src, Lock<AddRemove> lock, Entity dst);
} // namespace ecs
