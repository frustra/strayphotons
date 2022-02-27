#include "Controller.hh"

#include "ecs/EcsImpl.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<CharacterController>::Load(sp::Scene *scene,
        CharacterController &controller,
        const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "target") {
                auto fullTargetName = param.second.get<string>();
                ecs::Name targetName;
                if (targetName.Parse(param.second.get<string>(), scene)) {
                    controller.target = NamedEntity(targetName);
                } else {
                    Errorf("Invalid character controller target name: %s", fullTargetName);
                    return false;
                }
            } else if (param.first == "fallback_target") {
                auto fullTargetName = param.second.get<string>();
                ecs::Name fallbackName;
                if (fallbackName.Parse(param.second.get<string>(), scene)) {
                    controller.fallbackTarget = NamedEntity(fallbackName);
                } else {
                    Errorf("Invalid character controller fallback name: %s", fullTargetName);
                    return false;
                }
            } else if (param.first == "movement_proxy") {
                auto fullProxyName = param.second.get<string>();
                ecs::Name proxyName;
                if (proxyName.Parse(param.second.get<string>(), scene)) {
                    controller.movementProxy = NamedEntity(proxyName);
                } else {
                    Errorf("Invalid character controller proxy name: %s", fullProxyName);
                    return false;
                }
            }
        }
        return true;
    }
} // namespace ecs
