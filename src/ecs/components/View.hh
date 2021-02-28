#pragma once

#include <bitset>

#include <ecs/Components.hh>
#include <glm/glm.hpp>

namespace ecs {
    class Entity;
    template<typename>
    class Handle;

    using ClearModeStorage = std::bitset<4>;
    using ClearModeValue = const std::bitset<4>;

    // Define an enum type we can use on generic Views for describing the view clear mode.
    // This helps decouple the core of the engine from a particular graphics backend.
    namespace ClearMode {
        ClearModeValue None                      (0);
        ClearModeValue ColorBuffer          (1 << 0);
        ClearModeValue DepthBuffer          (1 << 1);
        ClearModeValue AccumulationBuffer   (1 << 2);
        ClearModeValue StencilBuffer        (1 << 3);

        static bool hasClearMode(const ClearModeStorage& storage, ClearModeValue& value) {
            return (storage & value) == value;
        }
    };

    class View {
    public:
        enum ViewType {
            VIEW_TYPE_PANCAKE,
            VIEW_TYPE_XR,
            VIEW_TYPE_LIGHT,
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

        // Optional parameters;
        glm::ivec2 offset = {0, 0};
        // TODO(any): Maybe remove color clear once we have interior spaces
        ClearModeStorage clearMode = (ClearMode::ColorBuffer | ClearMode::DepthBuffer);
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

    static Component<View> ComponentView("view");

    template<>
    bool Component<View>::Load(Lock<Read<ecs::Name>> lock, View &dst, const picojson::value &src);

    void ValidateView(Entity viewEntity);
    Handle<ecs::View> UpdateViewCache(Entity entity, float fov = 0.0);
} // namespace ecs
