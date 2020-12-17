#pragma once

#include "assets/Model.hh"

#include <ecs/Components.hh>

namespace ecs {
    struct Renderable {
        Renderable() {}
        Renderable(shared_ptr<sp::Model> model) : model(model) {}

        shared_ptr<sp::Model> model;
        bool hidden = false;
        bool xrExcluded = false; // Do not render this on XR Views
        glm::vec3 emissive = {0.0f, 0.0f, 0.0f};
        glm::vec3 voxelEmissive = {0.0f, 0.0f, 0.0f};

        bool operator==(const Renderable &other) const {
            return model == other.model && hidden == other.hidden && xrExcluded == other.xrExcluded &&
                   emissive == other.emissive && voxelEmissive == other.voxelEmissive;
        }

        bool operator!=(const Renderable &other) const {
            return !(*this == other);
        }
    };

    static Component<Renderable> ComponentRenderable("renderable");

    template<>
    bool Component<Renderable>::Load(Lock<Read<ecs::Name>> lock, Renderable &dst, const picojson::value &src);
    template<>
    bool Component<Renderable>::Save(picojson::value &dst, const Renderable &src);
} // namespace ecs
