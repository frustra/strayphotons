#include "Renderable.hh"

#include <assets/AssetManager.hh>
#include <core/Logging.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Renderable>::Load(const EntityScope &scope, Renderable &renderable, const picojson::value &src) {
        if (!renderable.modelName.empty()) {
            renderable.model = sp::Assets().LoadGltf(renderable.modelName);
        }
        return true;
    }

    template<>
    void Component<Renderable>::Apply(const Renderable &src, Lock<AddRemove> lock, Entity dst) {
        auto &dstRenderable = dst.Get<Renderable>(lock);
        if (!dstRenderable.model && src.model) dstRenderable.model = src.model;
    }

    Renderable::Renderable(const std::string &modelName, size_t meshIndex)
        : modelName(modelName), meshIndex(meshIndex) {
        if (modelName.empty()) {
            visibility = VisibilityMask::None;
        } else {
            model = sp::Assets().LoadGltf(modelName);
        }
    }
} // namespace ecs
