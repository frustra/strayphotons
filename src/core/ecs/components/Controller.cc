#include "Controller.hh"

#include "ecs/EcsImpl.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<HumanController>::Load(Lock<Read<ecs::Name>> lock,
                                          HumanController &controller,
                                          const picojson::value &src) {
        return true;
    }

    template<>
    bool Component<CharacterController>::Load(Lock<Read<ecs::Name>> lock,
                                              CharacterController &controller,
                                              const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "target") controller.target = ecs::NamedEntity(param.second.get<string>());
        }
        return true;
    }
} // namespace ecs
