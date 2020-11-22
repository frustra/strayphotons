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
    };

    static Component<Renderable> ComponentRenderable("renderable");

    template<>
    bool Component<Renderable>::LoadEntity(Entity &dst, picojson::value &src);
} // namespace ecs
