/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "console/CVar.hh"
#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"

namespace sp::scripts {
    using namespace ecs;

    struct Flashlight {
        EntityRef parentEntity;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<Light, TransformTree>(lock)) return;

            auto &light = ent.Get<Light>(lock);

            SignalRef onRef(ent, "on");
            light.on = onRef.GetSignal(lock) >= 0.5;
            light.intensity = SignalRef(ent, "intensity").GetSignal(lock);
            light.spotAngle = glm::radians(SignalRef(ent, "angle").GetSignal(lock));

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                if (event.name == "/action/flashlight/toggle") {
                    onRef.SetValue(lock, light.on ? 0.0 : 1.0);
                    light.on = !light.on;
                } else if (event.name == "/action/flashlight/grab") {
                    auto &transform = ent.Get<TransformTree>(lock);
                    if (transform.parent) {
                        transform.pose = transform.GetGlobalTransform(lock);
                        transform.parent = EntityRef();
                    } else {
                        if (parentEntity) {
                            transform.pose.SetPosition(glm::vec3(0, -0.3, 0));
                            transform.pose.SetRotation(glm::quat());
                            transform.parent = parentEntity;
                        } else {
                            Errorf("Flashlight parent entity is invalid: %s", parentEntity.Name().String());
                        }
                    }
                }
            }
        }
    };
    StructMetadata MetadataFlashlight(typeid(Flashlight),
        "Flashlight",
        "",
        StructField::New("parent", &Flashlight::parentEntity));
    InternalScript<Flashlight> flashlight("flashlight",
        MetadataFlashlight,
        false,
        "/action/flashlight/toggle",
        "/action/flashlight/grab");

    struct SunScript {
        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<TransformTree>(lock)) return;

            auto &transform = ent.Get<TransformTree>(lock);

            SignalRef positionRef(ent, "position");
            auto sunPos = positionRef.GetSignal(lock);
            if (SignalRef(ent, "fix_position").GetSignal(lock) == 0.0) {
                float intervalSeconds = interval.count() / 1e9;
                sunPos += intervalSeconds * (0.05 + std::abs(sin(sunPos) * 0.1));
                if (sunPos > M_PI_2) sunPos = -M_PI_2;
                positionRef.SetValue(lock, sunPos);
            }

            transform.pose.SetRotation(glm::quat());
            transform.pose.Rotate(glm::radians(-90.0), glm::vec3(1, 0, 0));
            transform.pose.Rotate(sunPos, glm::vec3(0, 1, 0));
            transform.pose.SetPosition(glm::vec3(sin(sunPos) * 40.0, cos(sunPos) * 40.0, 0));
        }
    };
    StructMetadata MetadataSunScript(typeid(SunScript), "SunScript", "");
    InternalScript<SunScript> sun("sun", MetadataSunScript);
} // namespace sp::scripts
