#pragma once

#include "assets/Model.hh"

#include <ecs/Components.hh>

namespace ecs {
    class Renderable {
    public:
        enum Visibility {
            VISIBILE_DIRECT_CAMERA = 0,
            VISIBILE_DIRECT_EYE = 0,
            VISIBILE_REFLECTED,
            VISIBILE_LIGHTING,
            VISIBILITY_COUNT,
        };

        using VisibilityMask = std::bitset<VISIBILITY_COUNT>;

        Renderable() {}
        Renderable(shared_ptr<sp::Model> model) : model(model) {}

    private:
        shared_ptr<sp::Model> model;
        VisibilityMask visibility = VisibilityMask().set();
        glm::vec3 emissive = {0.0f, 0.0f, 0.0f};
    };

    static Component<Renderable> ComponentRenderable("renderable");

    template<>
    bool Component<Renderable>::Load(Lock<Read<ecs::Name>> lock, Renderable &dst, const picojson::value &src);
} // namespace ecs
