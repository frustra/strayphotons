#pragma once

#include "assets/Async.hh"
#include "ecs/Components.hh"
#include "ecs/components/Events.hh"

namespace sp {
    class Asset;
}

namespace ecs {
    enum class SoundType {
        Object,
        Stereo,
        Ambisonic,
    };

    class Sound {
    public:
        // Set these fields before adding the Sound to the scene
        SoundType type = SoundType::Object;

        std::string filePath;
        sp::AsyncPtr<sp::Asset> file; // TODO: should make the asset system unpack the audio file
        bool loop = false, playOnLoad = false;

        // Update these fields at any point
        float volume = 1.0f;

        bool operator==(const Sound &) const = default;
    };

    static StructMetadata MetadataSound(typeid(Sound),
        StructField::New("type", &Sound::type),
        StructField::New("file", &Sound::filePath),
        StructField::New("loop", &Sound::loop),
        StructField::New("play_on_load", &Sound::playOnLoad),
        StructField::New("volume", &Sound::volume));
    template<>
    bool StructMetadata::Load<Sound>(const EntityScope &scope, Sound &dst, const picojson::value &src);

    struct Sounds {
        std::vector<Sound> sounds;
        EventQueueRef eventQueue;

        // Update these fields at any point
        float occlusion = 0.0f, occlusionWeight = 1.0f;
    };

    static StructMetadata MetadataSounds(typeid(Sounds), StructField::New(&Sounds::sounds, ~FieldAction::AutoApply));
    static Component<Sounds> ComponentSound("sound", MetadataSounds);

    template<>
    void Component<Sounds>::Apply(Sounds &dst, const Sounds &src, bool liveTarget);
} // namespace ecs
