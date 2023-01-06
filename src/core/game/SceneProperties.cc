#include "SceneProperties.hh"

#include "assets/JsonHelpers.hh"

namespace sp {
    glm::vec3 stationSpinFunc(glm::vec3 position) {
        position.z = 0;
        // Derived from centripetal acceleration formula, rotating around the origin
        const float spinRpm = 2.42f; // Calculated for ~1G at 153m radius
        float spinTerm = M_PI * spinRpm / 30;
        return spinTerm * spinTerm * position;
    }

    bool SceneProperties::operator==(const SceneProperties &other) const {
        auto *target = gravityFunction.target<glm::vec3 (*)(glm::vec3)>();
        auto *otherTarget = other.gravityFunction.target<glm::vec3 (*)(glm::vec3)>();
        return gravityTransform == other.gravityTransform && fixedGravity == other.fixedGravity &&
               target == otherTarget;
    }
} // namespace sp

namespace ecs {
    using namespace sp;

    template<>
    bool StructMetadata::Load<SceneProperties>(const EntityScope &scope,
        SceneProperties &dst,
        const picojson::value &src) {
        if (!src.is<picojson::object>()) {
            Errorf("Invalid scene properties: %s", src.to_str());
            return false;
        }

        for (auto &property : src.get<picojson::object>()) {
            if (property.first == "gravity_func") {
                if (property.second.is<std::string>()) {
                    auto gravityFunc = property.second.get<std::string>();
                    if (gravityFunc == "station_spin") {
                        // TODO: Define this function using a signal expression
                        dst.gravityFunction = &stationSpinFunc;
                    } else {
                        Errorf("SceneProperties unknown gravity_func: %s", gravityFunc);
                        return false;
                    }
                } else {
                    Errorf("SceneProperties invalid gravity_func: %s", property.second.to_str());
                    return false;
                }
            }
        }
        return true;
    }

    template<>
    void StructMetadata::Save<SceneProperties>(const EntityScope &scope,
        picojson::value &dst,
        const SceneProperties &src) {
        if (!src.gravityFunction) return;

        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &obj = dst.get<picojson::object>();

        auto *target = src.gravityFunction.target<glm::vec3 (*)(glm::vec3)>();
        if (target && *target == &stationSpinFunc) {
            json::Save(scope, obj["gravity_func"], "station_spin"s);
        } else {
            Abortf("Failed to serialize gravity function");
        }
    }
} // namespace ecs
