/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"
#include "ecs/ScriptImpl.hh"
#include "ecs/SignalStructAccess_common.hh"
#include "game/GameEntities.hh"
#include "strayphotons/HeapVector.hh"
#include "strayphotons/Logging.hh"

namespace sp::scripts {
    using namespace ecs;

    struct SoundOcclusion {
        PhysicsQuery::Handle<PhysicsQuery::Raycast> raycastQuery;

        void OnLogicTick(ScriptState &state,
            Lock<Read<TransformSnapshot>, Write<Audio, PhysicsQuery>> lock,
            Entity ent,
            chrono_clock::duration interval) {
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
    StructMetadata MetadataSoundOcclusion(typeid(SoundOcclusion), sizeof(SoundOcclusion), "SoundOcclusion", "");
    BuiltInScript<SoundOcclusion> soundOcclusion("sound_occlusion", MetadataSoundOcclusion);

    struct VolumeControl {
        SignalExpression volumeInput;
        SignalRef output;

        void OnLogicTick(ScriptState &state,
            Lock<ReadSignalsLock, Write<Audio, Signals>> lock,
            Entity ent,
            chrono_clock::duration interval) {
            if (volumeInput.expr.empty() || !ent.Has<Audio>(lock)) return;
            HeapVector<Sound> &sounds = ent.Get<Audio>(lock).sounds;
            if (sounds.empty()) return;

            if (!output) output = SignalRef(ent, "volume");

            sounds[0].volume = glm::clamp<float>(volumeInput.Evaluate(lock), 0.0f, 1.0f);
            output.SetValue(lock, sounds[0].volume);
        }

        void OnEvent(ScriptState &state, Lock<Write<Audio, Signals>> lock, Entity ent, Event event) {
            if (event.name != "/volume/set") return;
            if (!ent.Has<Audio>(lock)) return;
            HeapVector<Sound> &sounds = ent.Get<Audio>(lock).sounds;
            if (sounds.empty()) return;

            if (!output) output = SignalRef(ent, "volume");

            if (!volumeInput.expr.empty()) {
                Warnf("volume_control script does not support both expression and event input, "
                      "ignoring volume event for: %s",
                    EntityRef(ent).Name().String());
                return;
            }

            bool success = ecs::detail::ConvertAccessor<const float>(event.data, [&](const float &value) {
                sounds[0].volume = glm::clamp(value, 0.0f, 1.0f);
            });
            if (!success) {
                Logf("Couldn't convert from %s to float", event.ToString());
            } else {
                output.SetValue(lock, sounds[0].volume);
            }
        }
    };
    StructMetadata MetadataVolumeControl(typeid(VolumeControl),
        sizeof(VolumeControl),
        "VolumeControl",
        "",
        StructField::New("volume", &VolumeControl::volumeInput));
    BuiltInScript<VolumeControl> volumeControl("volume_control", MetadataVolumeControl, "/volume/set");

    struct SpeedControlledSound {
        bool init = false;
        Transform lastTransform;
        bool playing = false;
        int frames = 0;
        float avgSpeed = 0.0f;

        void OnLogicTick(ScriptState &state,
            DynamicLock<Read<TransformSnapshot>, SendEventsLock, Write<Audio>> lock,
            Entity ent,
            chrono_clock::duration interval) {
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
    StructMetadata MetadataSpeedControlledSound(typeid(SpeedControlledSound),
        sizeof(SpeedControlledSound),
        "SpeedControlledSound",
        "");
    BuiltInScript<SpeedControlledSound> speedControlledSound("speed_controlled_sound", MetadataSpeedControlledSound);
} // namespace sp::scripts
