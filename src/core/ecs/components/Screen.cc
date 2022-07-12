#include "Screen.hh"

#include "assets/JsonHelpers.hh"

namespace ecs {
    template<>
    bool Component<Screen>::Load(const EntityScope &scope, Screen &dst, const picojson::value &src) {
        if (src.is<string>()) {
            dst.textureName = src.get<string>();
        } else {
            for (auto param : src.get<picojson::object>()) {
                if (param.first == "target") {
                    dst.textureName = param.second.get<string>();
                } else if (param.first == "luminance") {
                    if (!sp::json::Load(scope, dst.luminanceScale, param.second)) {
                        Errorf("Invalid screen luminance: %s", param.second.to_str());
                        return false;
                    }
                }
            }
        }
        return true;
    }
} // namespace ecs
