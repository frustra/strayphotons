#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"
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

        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            if (!entLock.Has<TransformSnapshot, Sounds>()) return;
            auto &transform = entLock.Get<TransformSnapshot>();
            auto &sounds = entLock.Get<Sounds>();

            if (!init) {
                lastTransform = transform;
                init = true;
            }

            float delta = transform.GetPosition().y - lastTransform.GetPosition().y;
            bool shouldPlay = abs(delta) > 1e-8;
            if (shouldPlay || frames++ > 69) {
                if (shouldPlay != playing) {
                    if (shouldPlay) {
                        EventBindings::SendEvent(entLock, Event{"/sound/play", entLock.entity, 0});
                    } else {
                        EventBindings::SendEvent(entLock, Event{"/sound/stop", entLock.entity, 0});
                    }
                    playing = shouldPlay;
                }
                lastTransform = transform;
                frames = 0;
                avgSpeed = 0.9 * avgSpeed + 0.1 * delta;
                sounds.sounds[0].volume = std::min(1.0f, abs(avgSpeed) / 0.5f);
            }
        }
    };
    StructMetadata MetadataElevator(typeid(Elevator));
    InternalScript<Elevator> elevator("elevator", MetadataElevator);
} // namespace sp::scripts
