#pragma once

#include <bitset>
#include <ecs/Components.hh>
#include <glm/glm.hpp>

namespace ecs {
    struct View {
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
        View(glm::ivec2 extents, float fov = 0.0f, glm::vec2 clip = {0.1, 256}, ViewType viewType = VIEW_TYPE_CAMERA)
            : extents(extents), fov(fov), clip(clip), viewType(viewType) {}

        // Optional parameters;
        glm::ivec2 offset = {0, 0};

        // TODO(any): Maybe remove color clear once we have interior spaces
        ClearModeBitset clearMode =
            ClearModeBitset().set(CLEAR_MODE_COLOR_BUFFER, true).set(CLEAR_MODE_DEPTH_BUFFER, true);
        glm::vec4 clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
        bool stencil = false;
        bool blend = false;
        float skyIlluminance = 0.0f;

        // Required parameters.
        glm::ivec2 extents = {0, 0};
        float fov = 0.0f;
        glm::vec2 clip = {0.1, 256}; // {near, far}
        ViewType viewType = VIEW_TYPE_UNKNOWN;

        void UpdateMatrixCache(Lock<Read<Transform>> lock, Tecs::Entity e);

        // Matrix cache
        float aspect = 1.0f;
        glm::mat4 projMat, invProjMat;
        glm::mat4 viewMat, invViewMat;
    };

    static Component<View> ComponentView("view");

    template<>
    bool Component<View>::Load(Lock<Read<ecs::Name>> lock, View &dst, const picojson::value &src);
} // namespace ecs
