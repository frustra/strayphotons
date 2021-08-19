#include "Renderable.hh"

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
                if (param.first == "visibility") {
                    // TODO
                    // r.visibility = param.second;
                } else if (param.first == "model") {
                    r.model = sp::GAssets.LoadModel(param.second.get<string>());
                }
            }
        }
        if (!r.model) {
            Errorf("Renderable must have a model");
            return false;
        }

        return true;
    }
} // namespace ecs