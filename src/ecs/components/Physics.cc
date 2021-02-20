#include "ecs/components/Physics.hh"

#include <assets/AssetHelpers.hh>
#include <assets/AssetManager.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Physics>::Load(Lock<Read<ecs::Name>> lock, Physics &physics, const picojson::value &src) {
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

    template<>
    bool Component<Physics>::Save(picojson::value &dst, const Physics &src) {
        picojson::object physics;
        if (src.model) { physics["model"] = picojson::value(src.model->name); }
        physics["dynamic"] = picojson::value(src.desc.dynamic);
        physics["kinematic"] = picojson::value(src.desc.kinematic);
        physics["decomposeHull"] = picojson::value(src.desc.decomposeHull);
        physics["density"] = picojson::value(src.desc.density);
        dst.set<picojson::object>(physics);
        return true;
    }
} // namespace ecs
