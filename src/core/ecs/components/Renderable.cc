#include "Renderable.hh"

#include <assets/AssetManager.hh>
#include <core/Logging.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Renderable>::Load(const EntityScope &scope, Renderable &r, const picojson::value &src) {
        if (src.is<string>()) {
            r.model = sp::GAssets.LoadGltf(src.get<string>());
            r.meshIndex = 0;
        } else {
            for (auto param : src.get<picojson::object>()) {
                if (param.first == "hidden") {
                    if (param.second.get<bool>()) r.visibility = VisibilityMask::None;
                } else if (param.first == "model") {
                    r.model = sp::GAssets.LoadGltf(param.second.get<string>());
                } else if (param.first == "mesh_index") {
                    r.meshIndex = (size_t)param.second.get<double>();
                }
            }
        }
        r.visibility &= ~VisibilityMask::OutlineSelection;
        if (!r.model && r.visibility != VisibilityMask::None) {
            Errorf("Visible renderables must have a model");
            return false;
        }

        return true;
    }
} // namespace ecs
