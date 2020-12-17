#include "ecs/components/Renderable.hh"

#include <assets/AssetHelpers.hh>
#include <assets/AssetManager.hh>
#include <core/Logging.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Renderable>::Load(Lock<Read<ecs::Name>> lock, Renderable &r, const picojson::value &src) {
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

    template<>
    bool Component<Renderable>::Save(picojson::value &dst, const Renderable &src) {
        if (src.model) { dst.set(src.model->name); }
        return true;
    }
} // namespace ecs
