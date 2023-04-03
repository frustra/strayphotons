#include "console/CVar.hh"
#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"
#include "ecs/EntityReferenceManager.hh"

namespace sp::scripts {
    using namespace ecs;

    struct Flashlight {
        EntityRef parentEntity;

        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            if (!entLock.Has<Light, TransformTree, SignalOutput>()) return;

            auto &light = entLock.Get<Light>();
            auto &signalComp = entLock.Get<SignalOutput>();

            light.on = signalComp.GetSignal("on") >= 0.5;
            light.intensity = signalComp.GetSignal("intensity");
            light.spotAngle = glm::radians(signalComp.GetSignal("angle"));

            Event event;
            while (EventInput::Poll(entLock, state.eventQueue, event)) {
                if (event.name == "/action/flashlight/toggle") {
                    signalComp.SetSignal("on", light.on ? 0.0 : 1.0);
                    light.on = !light.on;
                } else if (event.name == "/action/flashlight/grab") {
                    auto &transform = entLock.Get<TransformTree>();
                    if (transform.parent) {
                        transform.pose = transform.GetGlobalTransform(entLock);
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
    StructMetadata MetadataFlashlight(typeid(Flashlight), StructField::New("parent", &Flashlight::parentEntity));
    InternalScript<Flashlight> flashlight("flashlight",
        MetadataFlashlight,
        false,
        "/action/flashlight/toggle",
        "/action/flashlight/grab");

    struct SunScript {
        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            if (!entLock.Has<TransformTree, SignalOutput>()) return;

            auto &transform = entLock.Get<TransformTree>();
            auto &signalComp = entLock.Get<SignalOutput>();

            auto sunPos = signalComp.GetSignal("position");
            if (signalComp.GetSignal("fix_position") == 0.0) {
                float intervalSeconds = interval.count() / 1e9;
                sunPos += intervalSeconds * (0.05 + std::abs(sin(sunPos) * 0.1));
                if (sunPos > M_PI_2) sunPos = -M_PI_2;
                signalComp.SetSignal("position", sunPos);
            }

            transform.pose.SetRotation(glm::quat());
            transform.pose.Rotate(glm::radians(-90.0), glm::vec3(1, 0, 0));
            transform.pose.Rotate(sunPos, glm::vec3(0, 1, 0));
            transform.pose.SetPosition(glm::vec3(sin(sunPos) * 40.0, cos(sunPos) * 40.0, 0));
        }
    };
    StructMetadata MetadataSunScript(typeid(SunScript));
    InternalScript<SunScript> sun("sun", MetadataSunScript);

    struct LightSensorScript {
        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            if (!entLock.Has<LightSensor, SignalOutput>()) return;

            auto &sensorComp = entLock.Get<LightSensor>();
            auto &outputComp = entLock.Get<SignalOutput>();

            outputComp.SetSignal("light_value_r", sensorComp.illuminance.r);
            outputComp.SetSignal("light_value_g", sensorComp.illuminance.g);
            outputComp.SetSignal("light_value_b", sensorComp.illuminance.b);
        }
    };
    StructMetadata MetadataLightSensorScript(typeid(LightSensorScript));
    InternalScript<LightSensorScript> lightSensor("light_sensor", MetadataLightSensorScript);
} // namespace sp::scripts
