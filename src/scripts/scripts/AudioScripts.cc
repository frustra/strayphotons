/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Common.hh"
#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/GameEntities.hh"
#include "game/Scene.hh"

namespace sp::scripts {
    using namespace ecs;

    struct SoundOcclusion {
        PhysicsQuery::Handle<PhysicsQuery::Raycast> raycastQuery;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<Audio, PhysicsQuery, TransformSnapshot>(lock)) return;

            auto &constAudio = ent.Get<const Audio>(lock);
            if (constAudio.occlusionWeight <= 0.0f) return;

            auto listener = sp::entities::Head.Get(lock);
            if (listener.Has<TransformSnapshot>(lock)) {
                auto &listenerPos = listener.Get<TransformSnapshot>(lock).globalPose.GetPosition();
                auto &soundPos = ent.Get<TransformSnapshot>(lock).globalPose.GetPosition();
                auto rayToListener = listenerPos - soundPos;

                PhysicsQuery::Raycast nextQuery(glm::length(rayToListener),
                    PhysicsGroupMask(PHYSICS_GROUP_WORLD | PHYSICS_GROUP_INTERACTIVE | PHYSICS_GROUP_HELD_OBJECT |
                                     PHYSICS_GROUP_PLAYER_LEFT_HAND | PHYSICS_GROUP_PLAYER_RIGHT_HAND));
                nextQuery.direction = glm::normalize(rayToListener);
                nextQuery.relativeDirection = false;
                nextQuery.maxHits = 16;

                auto &physicsQuery = ent.Get<PhysicsQuery>(lock);

                if (!raycastQuery) {
                    raycastQuery = physicsQuery.NewQuery(nextQuery);
                } else {
                    auto &query = physicsQuery.Lookup(raycastQuery);
                    if (query.result) {
                        auto &audio = ent.Get<Audio>(lock);
                        audio.occlusion = query.result->hits;
                        query = nextQuery;
                    }
                }
            }
        }
    };
    StructMetadata MetadataSoundOcclusion(typeid(SoundOcclusion), "SoundOcclusion", "");
    InternalScript<SoundOcclusion> soundOcclusion("sound_occlusion", MetadataSoundOcclusion);

    struct SpeedControlledSound {
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
    StructMetadata MetadataSpeedControlledSound(typeid(SpeedControlledSound), "SpeedControlledSound", "");
    InternalScript<SpeedControlledSound> elevator("speed_controlled_sound", MetadataSpeedControlledSound);
} // namespace sp::scripts
