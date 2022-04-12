#include "OpticalElement.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<OpticalElement>::Load(const EntityScope &scope, OpticalElement &optic, const picojson::value &src) {
        if (src.is<std::string>()) {
            auto typeStr = src.get<std::string>();
            sp::to_lower(typeStr);
            if (typeStr == "tint" || typeStr == "gel") {
                optic.type = OpticType::Gel;
            } else if (typeStr == "mirror") {
                optic.type = OpticType::Mirror;
            } else {
                Errorf("Unknown optic type: %s", typeStr);
                return false;
            }
        } else if (src.is<picojson::array>()) {
            optic.type = OpticType::Gel;
            optic.tint = sp::MakeVec3(src);
        } else if (src.is<picojson::object>()) {
            for (auto param : src.get<picojson::object>()) {
                if (param.first == "tint") {
                    optic.tint = sp::MakeVec3(param.second);
                } else if (param.first == "type") {
                    auto typeStr = param.second.get<std::string>();
                    sp::to_lower(typeStr);
                    if (typeStr == "tint" || typeStr == "gel") {
                        optic.type = OpticType::Gel;
                    } else if (typeStr == "mirror") {
                        optic.type = OpticType::Mirror;
                    } else {
                        Errorf("Unknown optic type: %s", typeStr);
                        return false;
                    }
                }
            }
        }
        return true;
    }
} // namespace ecs
