#include "Script.hh"

#include <cmath>
#include <core/Logging.hh>
#include <ecs/EcsImpl.hh>
#include <glm/glm.hpp>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Script>::LoadEntity(Lock<AddRemove> lock, Tecs::Entity &dst, const picojson::value &src) {
        auto &script = dst.Set<Script>(lock);

        for (auto param : src.get<picojson::object>()) {
            if (param.first == "onTick") {
                auto scriptName = param.second.get<std::string>();
                if (scriptName == "sun") {
                    script.AddOnTick([dst](ecs::Lock<ecs::WriteAll> lock, double dtSinceLastFrame) {
                        const Tecs::Entity &sun = dst;
                        if (sun && sun.Has<Transform, SignalOutput>(lock)) {
                            auto &transform = sun.Get<Transform>(lock);
                            auto &signalComp = sun.Get<SignalOutput>(lock);

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
                    });
                } else if (scriptName == "light_sensor") {
                    script.AddOnTick([dst](ecs::Lock<ecs::WriteAll> lock, double dtSinceLastFrame) {
                        const Tecs::Entity &sensor = dst;
                        if (sensor && sensor.Has<Script, LightSensor, SignalOutput>(lock)) {
                            auto &scriptComp = sensor.Get<Script>(lock);
                            auto &sensorComp = sensor.Get<LightSensor>(lock);
                            auto &outputComp = sensor.Get<SignalOutput>(lock);

                            outputComp.SetSignal("light_value_r", sensorComp.illuminance.r);
                            outputComp.SetSignal("light_value_g", sensorComp.illuminance.g);
                            outputComp.SetSignal("light_value_b", sensorComp.illuminance.b);
                            auto triggerParam = scriptComp.GetParam<double>("trigger_level");
                            bool enabled = glm::all(
                                glm::greaterThanEqual(sensorComp.illuminance, glm::vec3(std::abs(triggerParam))));
                            if (triggerParam < 0) { enabled = !enabled; }
                            outputComp.SetSignal("value", enabled ? 1.0 : 0.0);

                            // add emissiveness to sensor when it is active
                            if (sensor.Has<Renderable>(lock)) {
                                auto &renderable = sensor.Get<Renderable>(lock);
                                if (triggerParam >= 0) {
                                    renderable.emissive = enabled ? glm::vec3(0, 1, 0) : glm::vec3(0);
                                } else {
                                    renderable.emissive = enabled ? glm::vec3(0) : glm::vec3(1, 0, 0);
                                }
                            }
                        }
                    });
                } else if (scriptName == "event_multiply") {
                    script.AddOnTick([dst](ecs::Lock<ecs::WriteAll> lock, double dtSinceLastFrame) {
                        const Tecs::Entity &proxy = dst;
                        if (proxy && proxy.Has<Name, Script, EventInput, EventBindings>(lock)) {
                            auto &scriptComp = proxy.Get<Script>(lock);
                            auto &eventInput = proxy.Get<EventInput>(lock);
                            auto &eventBindings = proxy.Get<EventBindings>(lock);

                            Event event;
                            while (eventInput.Poll("script_input", event)) {
                                std::visit(
                                    [&eventBindings, &lock, &event, &scriptComp](auto &&arg) {
                                        using T = std::decay_t<decltype(arg)>;
                                        if constexpr (std::is_same_v<T, int>) {
                                            auto factorParam = scriptComp.GetParam<double>("multiply_factor");
                                            eventBindings.SendEvent(lock,
                                                                    "script_output",
                                                                    event.source,
                                                                    (int)(arg * factorParam));
                                        } else if constexpr (std::is_same_v<T, double>) {
                                            auto factorParam = scriptComp.GetParam<double>("multiply_factor");
                                            eventBindings.SendEvent(lock,
                                                                    "script_output",
                                                                    event.source,
                                                                    (double)(arg * factorParam));
                                        } else if constexpr (std::is_same_v<T, glm::vec2>) {
                                            auto factorParamX = scriptComp.GetParam<double>("multiply_factor_x");
                                            auto factorParamY = scriptComp.GetParam<double>("multiply_factor_y");
                                            eventBindings.SendEvent(
                                                lock,
                                                "script_output",
                                                event.source,
                                                arg * glm::vec2((float)factorParamX, (float)factorParamY));
                                        } else if constexpr (std::is_same_v<T, glm::vec3>) {
                                            auto factorParamX = scriptComp.GetParam<double>("multiply_factor_x");
                                            auto factorParamY = scriptComp.GetParam<double>("multiply_factor_y");
                                            auto factorParamZ = scriptComp.GetParam<double>("multiply_factor_z");
                                            eventBindings.SendEvent(lock,
                                                                    "script_output",
                                                                    event.source,
                                                                    arg * glm::vec3((float)factorParamX,
                                                                                    (float)factorParamY,
                                                                                    (float)factorParamZ));
                                        } else {
                                            sp::Abort("Unsupported event type: " + std::string(typeid(T).name()));
                                        }
                                    },
                                    event.data);
                            }
                        }
                    });
                } else {
                    Errorf("Script has unknown onTick event: %s", scriptName);
                    return false;
                }
            } else if (param.first == "parameters") {
                for (auto scriptParam : param.second.get<picojson::object>()) {
                    if (scriptParam.second.is<std::string>()) {
                        script.SetParam(scriptParam.first, scriptParam.second.get<std::string>());
                    } else if (scriptParam.second.is<bool>()) {
                        script.SetParam(scriptParam.first, scriptParam.second.get<bool>());
                    } else {
                        script.SetParam(scriptParam.first, scriptParam.second.get<double>());
                    }
                }
            }
        }
        return true;
    }
} // namespace ecs
