/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "assets/AssetManager.hh"
#include "common/Common.hh"
#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

#include <cmath>
#include <glm/glm.hpp>

namespace sp::scripts {
    using namespace ecs;

    struct Elevator {
        bool init = false;
        Transform lastTransform;
        bool playing = false;
        int frames = 0;
        float avgSpeed = 0.0f;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<TransformSnapshot, Audio>(lock)) return;
            auto &transform = ent.Get<TransformSnapshot>(lock).globalPose;
            auto &audio = ent.Get<Audio>(lock);

            if (!init) {
                lastTransform = transform;
                init = true;
            }

            float delta = transform.GetPosition().y - lastTransform.GetPosition().y;
            bool shouldPlay = std::abs(delta) > 1e-8;
            if (shouldPlay || frames++ > 69) {
                if (shouldPlay != playing) {
                    if (shouldPlay) {
                        EventBindings::SendEvent(lock, ent, Event{"/sound/play", ent, 0});
                    } else {
                        EventBindings::SendEvent(lock, ent, Event{"/sound/stop", ent, 0});
                    }
                    playing = shouldPlay;
                }
                lastTransform = transform;
                frames = 0;
                avgSpeed = 0.9 * avgSpeed + 0.1 * delta;
                audio.sounds[0].volume = std::min(1.0f, std::abs(avgSpeed) / 0.5f);
            }
        }
    };
    StructMetadata MetadataElevator(typeid(Elevator), "Elevator", "");
    InternalScript<Elevator> elevator("elevator", MetadataElevator);
} // namespace sp::scripts
