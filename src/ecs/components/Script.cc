#include "Script.hh"

#include <core/Logging.hh>
#include <ecs/EcsImpl.hh>
#include <ecs/Signals.hh>
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
                    script.AddOnTick([dst](ECS &ecs, double dtSinceLastFrame) {
                        const Tecs::Entity &sun = dst;
                        auto lock = ecs.StartTransaction<Write<Transform, SignalOutput>>();
                        if (sun && sun.Has<Transform, SignalOutput>(lock)) {
                            auto &transform = sun.Get<Transform>(lock);
                            auto &signalComp = sun.Get<SignalOutput>(lock);

                            auto sunPos = signalComp.GetSignal("position");
                            if (signalComp.GetSignal("fix_position") == 0.0) {
                                sunPos += dtSinceLastFrame * (0.05 + std::abs(sin(sunPos) * 0.1));
                                if (sunPos > M_PI / 2.0) sunPos = -M_PI / 2.0;
                                signalComp.SetSignal("position", sunPos);
                            }

                            transform.SetRotate(glm::mat4());
                            transform.Rotate(glm::radians(-90.0), glm::vec3(1, 0, 0));
                            transform.Rotate(sunPos, glm::vec3(0, 1, 0));
                            transform.SetPosition(glm::vec3(sin(sunPos) * 40.0, cos(sunPos) * 40.0, 0));
                        }
                    });
                } else if (scriptName == "light_sensor") {
                    script.AddOnTick([dst](ECS &ecs, double dtSinceLastFrame) {
                        const Tecs::Entity &sensor = dst;
                        auto lock = ecs.StartTransaction<Read<Script, LightSensor>, Write<SignalOutput, Renderable>>();
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
                } else if (scriptName == "slide_door") {
                    script.AddOnTick([dst](ECS &ecs, double dtSinceLastFrame) {
                        const Tecs::Entity &door = dst;
                        auto lock = ecs.StartTransaction<Read<Name, Script, SignalOutput>, Write<Animation>>();
                        if (door && door.Has<Script>(lock)) {
                            auto &scriptComp = door.Get<Script>(lock);

                            bool open = FindSignal(lock, scriptComp.GetParam<std::string>("input")) > 0.0;

                            std::string leftPanelName = scriptComp.GetParam<std::string>("left");
                            if (!leftPanelName.empty()) {
                                auto leftEnt = ecs::EntityWith<Name>(lock, leftPanelName);
                                if (leftEnt && leftEnt.Has<Animation>(lock)) {
                                    leftEnt.Get<Animation>(lock).AnimateToState(open ? 1 : 0);
                                }
                            }

                            std::string rightPanelName = scriptComp.GetParam<std::string>("right");
                            if (!rightPanelName.empty()) {
                                auto rightEnt = ecs::EntityWith<Name>(lock, rightPanelName);
                                if (rightEnt && rightEnt.Has<Animation>(lock)) {
                                    rightEnt.Get<Animation>(lock).AnimateToState(open ? 1 : 0);
                                }
                            }
                        }
                    });
                } else if (scriptName == "combinator") {
                    script.AddOnTick([dst](ECS &ecs, double dtSinceLastFrame) {
                        const Tecs::Entity &combinator = dst;
                        auto lock = ecs.StartTransaction<Read<Name, Script>, Write<SignalOutput>>();
                        if (combinator && combinator.Has<Script, SignalOutput>(lock)) {
                            auto &scriptComp = combinator.Get<Script>(lock);

                            double input_a = FindSignal(lock, scriptComp.GetParam<std::string>("input_a"));
                            double input_b = FindSignal(lock, scriptComp.GetParam<std::string>("input_b"));
                            auto &output = combinator.Get<SignalOutput>(lock);

                            auto operation = scriptComp.GetParam<std::string>("operation");
                            if (operation == "if") {
                                output.SetSignal("value", input_b > 0.0 ? input_a : 0.0);
                            } else if (operation == "not_if") {
                                output.SetSignal("value", input_b > 0.0 ? 0.0 : input_a);
                            } else if (operation == "greater") {
                                output.SetSignal("value", input_a > input_b ? 1.0 : 0.0);
                            } else if (operation == "greater_equal") {
                                output.SetSignal("value", input_a >= input_b ? 1.0 : 0.0);
                            } else if (operation == "and") {
                                output.SetSignal("value", (input_a > 0.0 && input_b > 0.0) ? 1.0 : 0.0);
                            } else if (operation == "or") {
                                output.SetSignal("value", (input_a > 0.0 || input_b > 0.0) ? 1.0 : 0.0);
                            } else if (operation == "add") {
                                output.SetSignal("value", input_a + input_b);
                            } else if (operation == "substract") {
                                output.SetSignal("value", input_a - input_b);
                            } else {
                                throw std::runtime_error("Unknown combinator operation: " + operation);
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
