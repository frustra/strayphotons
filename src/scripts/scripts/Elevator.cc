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

    InternalScript elevatorScript("elevator",
        [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<EventInput, TransformSnapshot, Sounds>(lock)) return;
            auto &transform = ent.Get<TransformSnapshot>(lock);
            auto &sounds = ent.Get<Sounds>(lock);

            struct Elevator {
                Transform lastTransform;
                bool playing = false;
                int frames = 0;
                float avgSpeed = 0.0f;
            };

            Elevator elevator;
            if (state.userData.has_value()) {
                elevator = std::any_cast<Elevator>(state.userData);
            } else {
                elevator.lastTransform = transform;
            }

            float delta = transform.GetPosition().y - elevator.lastTransform.GetPosition().y;
            bool shouldPlay = abs(delta) > 1e-8;
            if (shouldPlay || elevator.frames++ > 69) {
                if (shouldPlay != elevator.playing) {
                    if (shouldPlay) {
                        EventBindings::SendEvent(lock, "/sound/play", ent, 0);
                    } else {
                        EventBindings::SendEvent(lock, "/sound/stop", ent, 0);
                    }
                    elevator.playing = shouldPlay;
                }
                elevator.lastTransform = transform;
                elevator.frames = 0;
                elevator.avgSpeed = 0.9 * elevator.avgSpeed + 0.1 * delta;
                sounds.sounds[0].volume = std::min(1.0f, abs(elevator.avgSpeed) / 0.5f);
            }
            state.userData = elevator;
        });
} // namespace sp::scripts
