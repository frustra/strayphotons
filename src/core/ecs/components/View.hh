#pragma once

#include <bitset>
#include <ecs/Components.hh>
#include <glm/glm.hpp>

namespace ecs {
    class Entity;
    template<typename>
    class Handle;

    class View {
    public:
        enum ViewType {
            VIEW_TYPE_UNKNOWN = 0,
            VIEW_TYPE_CAMERA,
            VIEW_TYPE_EYE, // XR / Stereo views
            VIEW_TYPE_SHADOW_MAP,
        };

        // Define a std::bitset and a corresponding enum that can be used to store clear modes independent of any
        // graphics backend.
        enum ClearMode {
            CLEAR_MODE_COLOR_BUFFER = 0,
            CLEAR_MODE_DEPTH_BUFFER,
            CLEAR_MODE_ACCUMULATION_BUFFER,
            CLEAR_MODE_STENCIL_BUFFER,
            CLEAR_MODE_COUNT,
        };

        using ClearModeBitset = std::bitset<CLEAR_MODE_COUNT>;

        View() {}
        View(glm::ivec2 extents, float fov, ViewType viewType = VIEW_TYPE_CAMERA) : extents(extents) {}

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
        ClearModeBitset clearMode =
            ClearModeBitset().set(CLEAR_MODE_COLOR_BUFFER, true).set(CLEAR_MODE_DEPTH_BUFFER, true);
        glm::vec4 clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
        bool stencil = false;
        bool blend = false;
        float skyIlluminance = 0.0f;

        glm::vec2 clip = {0.1, 256}; // {near, far}

        // Required parameters.
        glm::ivec2 extents = {0, 0};
        float fov = 0.0f;
        ViewType viewType = VIEW_TYPE_UNKNOWN;

    private:
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
