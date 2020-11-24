#include "Script.hh"

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
                    script.AddOnTick([dst](ECS &ecs, double dtSinceLastFrame) {
                        static double sunPos = 0.0;
                        const Tecs::Entity &sun = dst;
                        auto lock = ecs.StartTransaction<Read<Script>, Write<Transform>>();
                        if (sun && sun.Has<Transform>(lock)) {
                            auto &scriptComp = sun.Get<Script>(lock);
                            auto sunPosParam = scriptComp.GetParam("sun_position");
                            if (sunPosParam == 0) {
                                sunPos += dtSinceLastFrame * (0.05 + std::abs(sin(sunPos) * 0.1));
                                if (sunPos > M_PI / 2.0) sunPos = -M_PI / 2.0;
                            } else {
                                sunPos = sunPosParam;
                            }

                            auto &transform = sun.Get<ecs::Transform>(lock);
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
                            auto triggerParam = scriptComp.GetParam("trigger_level");
                            bool enabled = glm::all(
                                glm::greaterThanEqual(sensorComp.illuminance, glm::vec3(std::abs(triggerParam))));
                            if (triggerParam < 0) { enabled = !enabled; }
                            outputComp.SetSignal("enabled", enabled ? 1.0 : 0.0);

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
                        auto lock = ecs.StartTransaction<Read<Script, SignalOutput>, Write<Animation>>();
                        if (door && door.Has<Script>(lock)) {
                            auto &scriptComp = door.Get<Script>(lock);

                            Tecs::Entity inputEnt((size_t)scriptComp.GetParam("input"));
                            if (inputEnt && inputEnt.Has<SignalOutput>(lock)) {
                                bool open = inputEnt.Get<SignalOutput>(lock).GetSignal("enabled") > 0.0;

                                Tecs::Entity leftEnt((size_t)scriptComp.GetParam("left"));
                                if (leftEnt && leftEnt.Has<Animation>(lock)) {
                                    leftEnt.Get<Animation>(lock).AnimateToState(open ? 1 : 0);
                                }
                                Tecs::Entity rightEnt((size_t)scriptComp.GetParam("right"));
                                if (rightEnt && rightEnt.Has<Animation>(lock)) {
                                    rightEnt.Get<Animation>(lock).AnimateToState(open ? 1 : 0);
                                }
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
                        auto entityName = scriptParam.second.get<string>();
                        for (auto ent : lock.EntitiesWith<Name>()) {
                            if (ent.Get<Name>(lock) == entityName) {
                                script.SetParam(scriptParam.first, (double)ent.id);
                                break;
                            }
                        }
                    } else if (scriptParam.second.is<bool>()) {
                        script.SetParam(scriptParam.first, scriptParam.second.get<bool>() ? 1.0 : 0.0);
                    } else {
                        script.SetParam(scriptParam.first, scriptParam.second.get<double>());
                    }
                }
            }
        }
        return true;
    }
} // namespace ecs
