#include "Light.hh"

#include "assets/JsonHelpers.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

namespace ecs {
    template<>
    bool Component<Light>::Load(const EntityScope &scope, Light &light, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "spotAngle") {
                light.spotAngle = glm::radians(param.second.get<double>());
            }
        }
        return true;
    }

    template<>
    void Component<Light>::Apply(const Light &src, Lock<AddRemove> lock, Entity dst) {
        static const Light defaultValues = {};
        auto &dstLight = dst.Get<Light>(lock);
        if (dstLight.intensity == defaultValues.intensity) dstLight.intensity = src.intensity;
        if (dstLight.illuminance == defaultValues.illuminance) dstLight.illuminance = src.illuminance;
        if (dstLight.spotAngle == defaultValues.spotAngle) dstLight.spotAngle = src.spotAngle;
        if (dstLight.tint == defaultValues.tint) dstLight.tint = src.tint;
        if (dstLight.gelName == defaultValues.gelName) dstLight.gelName = src.gelName;
        if (dstLight.on == defaultValues.on) dstLight.on = src.on;
        if (dstLight.shadowMapSize == defaultValues.shadowMapSize) dstLight.shadowMapSize = src.shadowMapSize;
        if (dstLight.shadowMapClip == defaultValues.shadowMapClip) dstLight.shadowMapClip = src.shadowMapClip;
    }
} // namespace ecs
