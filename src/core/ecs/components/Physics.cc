#include "Physics.hh"

#include <assets/AssetHelpers.hh>
#include <assets/AssetManager.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Physics>::Load(sp::Scene *scene, Physics &physics, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "model") {
                physics.model = sp::GAssets.LoadModel(param.second.get<string>());
            } else if (param.first == "dynamic") {
                physics.dynamic = param.second.get<bool>();
            } else if (param.first == "kinematic") {
                physics.kinematic = param.second.get<bool>();
            } else if (param.first == "decomposeHull") {
                physics.decomposeHull = param.second.get<bool>();
            } else if (param.first == "density") {
                physics.density = param.second.get<double>();
            }
        }
        return true;
    }
} // namespace ecs
