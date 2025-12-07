/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Sound.hh"

#include "assets/AssetManager.hh"
#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <picojson.h>

namespace ecs {
    template<>
    bool StructMetadata::Load<Sound>(Sound &sound, const picojson::value &src) {
        if (!sound.filePath.empty()) {
            sound.file = sp::Assets().Load("audio/" + sound.filePath);
        }
        return true;
    }

    template<>
    void EntityComponent<Audio>::Apply(Audio &dst, const Audio &src, bool liveTarget) {
        for (auto &sound : src.sounds) {
            if (!sp::contains(dst.sounds, sound)) dst.sounds.emplace_back(sound);
        }
    }
} // namespace ecs
