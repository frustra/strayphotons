#include "ecs/components/Renderable.hh"

#include <assets/AssetHelpers.hh>
#include <assets/AssetManager.hh>
#include <core/Logging.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Renderable>::LoadEntity(Lock<AddRemove> lock, Tecs::Entity &dst, const picojson::value &src) {
        auto &r = dst.Set<Renderable>(lock);
        if (src.is<string>()) {
            r.model = sp::GAssets.LoadModel(src.get<string>());
        } else {
            for (auto param : src.get<picojson::object>()) {
                if (param.first == "emissive") {
                    r.emissive = sp::MakeVec3(param.second);
                } else if (param.first == "light") {
                    r.voxelEmissive = sp::MakeVec3(param.second);
                } else if (param.first == "model") {
                    r.model = sp::GAssets.LoadModel(param.second.get<string>());
                }
            }
        }
        if (!r.model) {
            Errorf("Renderable must have a model");
            return false;
        }

        if (glm::length(r.emissive) == 0.0f && glm::length(r.voxelEmissive) > 0.0f) { r.emissive = r.voxelEmissive; }
        return true;
    }
} // namespace ecs
