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
            VisibilityMask mask = VisibilityMask::None)
            : extents(extents), fov(fov), clip(clip), visibilityMask(mask) {
            UpdateProjectionMatrix();
        }

        // Optional parameters;
        glm::ivec2 offset = {0, 0};

        // Required parameters.
        glm::ivec2 extents = {0, 0};
        sp::angle_t fov = 0.0f;
        glm::vec2 clip = {0.1, 256}; // {near, far}
        VisibilityMask visibilityMask = VisibilityMask::None;

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

    static Component<View> ComponentView("view",
        ComponentField::New("offset", &View::offset),
        ComponentField::New("extents", &View::extents),
        ComponentField::New("fov", &View::fov),
        ComponentField::New("clip", &View::clip),
        ComponentField::New("visibilityMask", &View::visibilityMask));

    template<>
    bool Component<View>::Load(const EntityScope &scope, View &dst, const picojson::value &src);
    template<>
    void Component<View>::Apply(const View &src, Lock<AddRemove> lock, Entity dst);
} // namespace ecs
