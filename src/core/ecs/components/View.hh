/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

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

        explicit operator bool() const {
            return extents.x > 0 && extents.y > 0;
        }

        bool operator!() const {
            return extents.x <= 0 || extents.y <= 0;
        }

        // Matrix cache
        glm::mat4 projMat, invProjMat;
        glm::mat4 viewMat, invViewMat;
    };

    static Component<View> ComponentView({typeid(View),
        "view",
        "",
        StructField::New("offset", &View::offset),
        StructField::New("extents", &View::extents),
        StructField::New("fov", &View::fov),
        StructField::New("clip", &View::clip),
        StructField::New("visibility_mask", &View::visibilityMask)});

    template<>
    void Component<View>::Apply(View &dst, const View &src, bool liveTarget);
} // namespace ecs
