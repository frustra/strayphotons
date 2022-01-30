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
                controller.target = ecs::NamedEntity(param.second.get<string>());
            } else if (param.first == "movement_proxy") {
                controller.movementProxy = ecs::NamedEntity(param.second.get<string>());
            }
        }
        return true;
    }
} // namespace ecs
