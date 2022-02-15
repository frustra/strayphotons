#pragma once

#include "ecs/Components.hh"

namespace sp {
    class Asset;
}

namespace ecs {
    class AudioSource {
    public:
        shared_ptr<const sp::Asset> file; // TODO: should make the asset system unpack the audio file
    };

    static Component<AudioSource> ComponentAudioSource("audio_source");

    template<>
    bool Component<AudioSource>::Load(sp::Scene *scene, AudioSource &dst, const picojson::value &src);
} // namespace ecs
