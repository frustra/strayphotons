/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

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
        "Sound",
        "",
        StructField::New("type", &Sound::type),
        StructField::New("file", &Sound::filePath),
        StructField::New("loop", &Sound::loop),
        StructField::New("play_on_load", &Sound::playOnLoad),
        StructField::New("volume", &Sound::volume));
    template<>
    bool StructMetadata::Load<Sound>(Sound &dst, const picojson::value &src);

    struct Audio {
        std::vector<Sound> sounds;
        EventQueueRef eventQueue;

        // Update these fields at any point
        float occlusion = 0.0f, occlusionWeight = 1.0f;
    };

    static StructMetadata MetadataAudio(typeid(Audio),
        "audio",
        "",
        StructField::New(&Audio::sounds, ~FieldAction::AutoApply));
    static Component<Audio> ComponentAudio(MetadataAudio);

    template<>
    void Component<Audio>::Apply(Audio &dst, const Audio &src, bool liveTarget);
} // namespace ecs
