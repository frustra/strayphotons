#include "Script.hh"

#include <cmath>
#include <core/CVar.hh>
#include <core/Common.hh>
#include <core/Logging.hh>
#include <ecs/EcsImpl.hh>
#include <glm/glm.hpp>
#include <robin_hood.h>
#include <sstream>

namespace ecs {
    static sp::CVar<std::string> CVarFlashlightParent("r.FlashlightParent", "player", "Flashlight parent entity name");

    robin_hood::unordered_node_map<std::string, ScriptFunc> ScriptDefinitions = {
        {"flashlight",
         [](ecs::Lock<ecs::WriteAll> lock, Tecs::Entity ent, double dtSinceLastFrame) {
             if (ent.Has<Light, Transform, SignalOutput, EventInput>(lock)) {
                 auto &light = ent.Get<Light>(lock);
                 auto &signalComp = ent.Get<SignalOutput>(lock);

                 light.on = signalComp.GetSignal("on") >= 0.5;
                 light.intensity = signalComp.GetSignal("intensity");
                 light.spotAngle = glm::radians(signalComp.GetSignal("angle"));

                 ecs::Event event;
                 if (ecs::EventInput::Poll(lock, ent, "/action/flashlight/toggle", event)) {
                     signalComp.SetSignal("on", light.on ? 0.0 : 1.0);
                     light.on = !light.on;
                 }
                 if (ecs::EventInput::Poll(lock, ent, "/action/flashlight/grab", event)) {
                     auto &transform = ent.Get<Transform>(lock);
                     if (transform.HasParent(lock)) {
                         transform.SetPosition(transform.GetGlobalPosition(lock));
                         transform.SetRotation(transform.GetGlobalRotation(lock));
                         transform.SetParent(Tecs::Entity());
                     } else {
                         Tecs::Entity parent = ecs::EntityWith<ecs::Name>(lock, CVarFlashlightParent.Get());
                         if (parent) {
                             transform.SetPosition(glm::vec3(0, -0.3, 0));
                             transform.SetRotation(glm::quat());
                             transform.SetParent(parent);
                         } else {
                             Errorf("Flashlight parent entity does not exist: %s", CVarFlashlightParent.Get());
                         }
                     }
                     transform.UpdateCachedTransform(lock);
                 }
             }
         }},
        {"sun",
         [](ecs::Lock<ecs::WriteAll> lock, Tecs::Entity ent, double dtSinceLastFrame) {
             if (ent.Has<Transform, SignalOutput>(lock)) {
                 auto &transform = ent.Get<Transform>(lock);
                 auto &signalComp = ent.Get<SignalOutput>(lock);

                 auto sunPos = signalComp.GetSignal("position");
                 if (signalComp.GetSignal("fix_position") == 0.0) {
                     sunPos += dtSinceLastFrame * (0.05 + std::abs(sin(sunPos) * 0.1));
                     if (sunPos > M_PI_2) sunPos = -M_PI_2;
                     signalComp.SetSignal("position", sunPos);
                 }

                 transform.SetRotation(glm::quat());
                 transform.Rotate(glm::radians(-90.0), glm::vec3(1, 0, 0));
                 transform.Rotate(sunPos, glm::vec3(0, 1, 0));
                 transform.SetPosition(glm::vec3(sin(sunPos) * 40.0, cos(sunPos) * 40.0, 0));
             }
         }},
        {"light_sensor",
         [](ecs::Lock<ecs::WriteAll> lock, Tecs::Entity ent, double dtSinceLastFrame) {
             if (ent.Has<Script, LightSensor, SignalOutput>(lock)) {
                 auto &scriptComp = ent.Get<Script>(lock);
                 auto &sensorComp = ent.Get<LightSensor>(lock);
                 auto &outputComp = ent.Get<SignalOutput>(lock);

                 outputComp.SetSignal("light_value_r", sensorComp.illuminance.r);
                 outputComp.SetSignal("light_value_g", sensorComp.illuminance.g);
                 outputComp.SetSignal("light_value_b", sensorComp.illuminance.b);
                 auto triggerParam = scriptComp.GetParam<double>("trigger_level");
                 bool enabled = glm::all(
                     glm::greaterThanEqual(sensorComp.illuminance, glm::vec3(std::abs(triggerParam))));
                 if (triggerParam < 0) { enabled = !enabled; }
                 outputComp.SetSignal("value", enabled ? 1.0 : 0.0);

                 // add emissiveness to sensor when it is active
                 if (ent.Has<Renderable>(lock)) {
                     auto &renderable = ent.Get<Renderable>(lock);
                     if (triggerParam >= 0) {
                         renderable.emissive = enabled ? glm::vec3(0, 1, 0) : glm::vec3(0);
                     } else {
                         renderable.emissive = enabled ? glm::vec3(0) : glm::vec3(1, 0, 0);
                     }
                 }
             }
         }},
        {"joystick_calibration",
         [](ecs::Lock<ecs::WriteAll> lock, Tecs::Entity ent, double dtSinceLastFrame) {
             if (ent.Has<Name, Script, EventInput, EventBindings>(lock)) {
                 auto &scriptComp = ent.Get<Script>(lock);
                 auto &eventInput = ent.Get<EventInput>(lock);
                 auto &eventBindings = ent.Get<EventBindings>(lock);

                 Event event;
                 while (eventInput.Poll("/script/joystick_in", event)) {
                     auto data = std::get_if<glm::vec2>(&event.data);
                     if (data) {
                         float factorParamX = scriptComp.GetParam<double>("scale_x");
                         float factorParamY = scriptComp.GetParam<double>("scale_y");
                         eventBindings.SendEvent(lock,
                                                 "/script/joystick_out",
                                                 event.source,
                                                 glm::vec2(data->x * factorParamX, data->y * factorParamY));
                     } else {
                         Abortf("Unsupported event type: %s", event.toString());
                     }
                 }
             }
         }},
    };
} // namespace ecs
