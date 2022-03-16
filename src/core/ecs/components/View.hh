#pragma once

#include "ecs/Components.hh"
#include "ecs/components/Renderable.hh"

#include <bitset>
#include <glm/glm.hpp>

namespace ecs {
    struct View {
        View() {}
        View(glm::ivec2 extents,
            float fov = 0.0f,
            glm::vec2 clip = {0.1, 256},
            Renderable::VisibilityMask mask = Renderable::VisibilityMask())
            : extents(extents), fov(fov), clip(clip), visibilityMask(mask) {
            UpdateProjectionMatrix();
        }

        // Optional parameters;
        glm::ivec2 offset = {0, 0};

        // Required parameters.
        glm::ivec2 extents = {0, 0};
        float fov = 0.0f;
        glm::vec2 clip = {0.1, 256}; // {near, far}
        Renderable::VisibilityMask visibilityMask;

        void UpdateProjectionMatrix();
        void UpdateViewMatrix(Lock<Read<TransformSnapshot>> lock, Entity e);

        void SetProjMat(const glm::mat4 &newProjMat) {
            projMat = newProjMat;
            invProjMat = glm::inverse(projMat);
        }

        void SetInvViewMat(const glm::mat4 &newInvViewMat) {
            invViewMat = newInvViewMat;
            viewMat = glm::inverse(invViewMat);
        }

        // Matrix cache
        glm::mat4 projMat, invProjMat;
        glm::mat4 viewMat, invViewMat;
    };

    static Component<View> ComponentView("view");

    template<>
    bool Component<View>::Load(ScenePtr scenePtr, View &dst, const picojson::value &src);
} // namespace ecs
