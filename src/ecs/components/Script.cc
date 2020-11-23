#include "Script.hh"

#include <ecs/EcsImpl.hh>
#include <glm/glm.hpp>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Script>::LoadEntity(Entity &dst, picojson::value &src) {
        auto script = dst.Assign<Script>();

        for (auto param : src.get<picojson::object>()) {
            if (param.first == "onTick") {
                auto scriptName = param.second.get<std::string>();
                if (scriptName == "sun") {
                    script->AddOnTick([dst](ECS &ecs, double dtSinceLastFrame) {
                        static double sunPos = 0.0;
                        Tecs::Entity sun = dst.GetId();
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
                } else {
                    Errorf("Script has unknown onTick event: %s", scriptName);
                    return false;
                }
            } else if (param.first == "parameters") {
                for (auto scriptParam : param.second.get<picojson::object>()) {
                    script->SetParam(scriptParam.first, scriptParam.second.get<double>());
                }
            }
        }
        return true;
    }
} // namespace ecs
