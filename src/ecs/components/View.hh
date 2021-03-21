#pragma once

#include <ecs/Components.hh>
#include <glm/glm.hpp>

namespace ecs {
    class Entity;
    template<typename>
    class Handle;

    class View {
    public:
        enum ViewType {
            VIEW_TYPE_PANCAKE,
            VIEW_TYPE_XR,
            VIEW_TYPE_LIGHT,
        };

        // Define an enum type we can use on generic Views for describing the view clear mode.
        // This helps decouple the core of the engine from a particular graphics backend.
        //
        // NOTE: The ClearMode enum values are used as bit-flags! Ensure that any values added to this enum
        // are represented by a single unique bit.
        enum ClearMode { 
            None                = (0), 
            ColorBuffer         = (1 << 0), 
            DepthBuffer         = (1 << 1), 
            AccumulationBuffer  = (1 << 2), 
            StencilBuffer       = (1 << 3), 
        };

        View() {}
        View(glm::ivec2 extents) : extents(extents) {}

        // When setting these parameters, View needs to recompute some internal params
        void SetProjMat(glm::mat4 proj);
        void SetProjMat(float _fov, glm::vec2 _clip, glm::ivec2 _extents);
        void SetInvViewMat(glm::mat4 invView);

        glm::ivec2 GetExtents();
        glm::vec2 GetClip();
        float GetFov();

        glm::mat4 GetProjMat();
        glm::mat4 GetInvProjMat();
        glm::mat4 GetViewMat();
        glm::mat4 GetInvViewMat();

        bool HasClearMode(ClearMode mode) const;

        // Optional parameters;
        glm::ivec2 offset = {0, 0};
        // TODO(any): Maybe remove color clear once we have interior spaces
        ClearMode clearMode = (ClearMode) (ClearMode::ColorBuffer | ClearMode::DepthBuffer);
        glm::vec4 clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
        bool stencil = false;
        bool blend = false;
        float skyIlluminance = 0.0f;
        float scale = 1.0f;

        // For XR Views
        ViewType viewType = VIEW_TYPE_PANCAKE;

        // private:
        // Required parameters.
        glm::ivec2 extents = {0, 0};
        glm::vec2 clip = {0, 0}; // {near, far}
        float fov = 0.0f;

        // Updated automatically.
        float aspect = 1.0f;
        glm::mat4 projMat, invProjMat;
        glm::mat4 viewMat, invViewMat;
    };

    // Define some helpful operators for working with the ClearMode bit flags
    // Note: must be declared outside the scope of the View class or the compiler gets confused.
    // Note: cannot be moved above View because C++ doesn't support scoped forward declarations.
    using ClearMode_t = std::underlying_type<View::ClearMode>::type;
    inline View::ClearMode operator | (View::ClearMode lhs, View::ClearMode rhs) {
        return static_cast<View::ClearMode>(static_cast<ClearMode_t>(lhs) | static_cast<ClearMode_t>(rhs));
    }

    inline View::ClearMode operator & (View::ClearMode lhs, View::ClearMode rhs) {
        return static_cast<View::ClearMode>(static_cast<ClearMode_t>(lhs) & static_cast<ClearMode_t>(rhs));
    }

    inline View::ClearMode operator |= (View::ClearMode& lhs, View::ClearMode rhs) {
        return lhs = static_cast<View::ClearMode>(static_cast<ClearMode_t>(lhs) | static_cast<ClearMode_t>(rhs));
    }

    inline View::ClearMode operator &= (View::ClearMode& lhs, View::ClearMode rhs) {
        return lhs = static_cast<View::ClearMode>(static_cast<ClearMode_t>(lhs) & static_cast<ClearMode_t>(rhs));
    }

    inline View::ClearMode operator ~ (View::ClearMode lhs) {
        return static_cast<View::ClearMode>(~static_cast<ClearMode_t>(lhs));
    }

    static Component<View> ComponentView("view");

    template<>
    bool Component<View>::Load(Lock<Read<ecs::Name>> lock, View &dst, const picojson::value &src);

    void ValidateView(Entity viewEntity);
    Handle<ecs::View> UpdateViewCache(Entity entity, float fov = 0.0);
} // namespace ecs
