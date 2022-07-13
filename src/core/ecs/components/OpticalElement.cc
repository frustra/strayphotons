#include "OpticalElement.hh"

#include "assets/JsonHelpers.hh"
#include "ecs/EcsImpl.hh"

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
            if (!sp::json::Load(scope, optic.tint, src)) {
                Errorf("Invalid optic tint: %s", src.to_str());
                return false;
            }
        } else if (src.is<picojson::object>()) {
            for (auto param : src.get<picojson::object>()) {
                if (param.first == "tint") {
                    if (!sp::json::Load(scope, optic.tint, param.second)) {
                        Errorf("Invalid optic tint: %s", param.second.to_str());
                        return false;
                    }
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
