#include "ecs/components/Physics.hh"

#include <assets/AssetHelpers.hh>
#include <assets/AssetManager.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Physics>::LoadEntity(Lock<AddRemove> lock, Tecs::Entity &dst, const picojson::value &src) {
        auto &physics = dst.Set<Physics>(lock);
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "model") {
                physics.model = sp::GAssets.LoadModel(param.second.get<string>());
            } else if (param.first == "dynamic") {
                physics.desc.dynamic = param.second.get<bool>();
            } else if (param.first == "kinematic") {
                physics.desc.kinematic = param.second.get<bool>();
            } else if (param.first == "decomposeHull") {
                physics.desc.decomposeHull = param.second.get<bool>();
            } else if (param.first == "density") {
                physics.desc.density = param.second.get<double>();
            }
        }
        return true;
    }
} // namespace ecs
