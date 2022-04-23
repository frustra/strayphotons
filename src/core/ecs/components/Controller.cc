#include "Controller.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<CharacterController>::Load(const EntityScope &scope,
        CharacterController &controller,
        const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "target") {
                auto relativeName = param.second.get<string>();
                controller.target = ecs::Name(relativeName, scope.prefix);
                if (!controller.target) {
                    Errorf("Invalid character controller target name: %s", relativeName);
                    return false;
                }
            } else if (param.first == "fallback_target") {
                auto relativeName = param.second.get<string>();
                controller.fallbackTarget = ecs::Name(relativeName, scope.prefix);
                if (!controller.fallbackTarget) {
                    Errorf("Invalid character controller fallback name: %s", relativeName);
                    return false;
                }
            } else if (param.first == "movement_proxy") {
                auto relativeName = param.second.get<string>();
                controller.movementProxy = ecs::Name(relativeName, scope.prefix);
                if (!controller.movementProxy) {
                    Errorf("Invalid character controller proxy name: %s", relativeName);
                    return false;
                }
            }
        }
        return true;
    }
} // namespace ecs
