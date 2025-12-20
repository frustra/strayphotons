/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "View.hh"

#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace ecs {
    template<>
    void EntityComponent<View>::Apply(View &dst, const View &src, bool liveTarget) {
        if (src.projMat != glm::identity<glm::mat4>()) {
            dst.projMat = src.projMat;
            dst.invProjMat = src.invProjMat;
        } else {
            dst.UpdateProjectionMatrix();
        }
    }

    void View::UpdateProjectionMatrix() {
        if (fov > 0 && (extents.x != 0 || extents.y != 0)) {
            auto aspect = extents.x / (float)extents.y;

            this->projMat = glm::perspective(fov.radians(), aspect, clip[0], clip[1]);
            this->invProjMat = glm::inverse(projMat);
        } else if (this->extents.x != 0 || this->extents.y != 0) {
            this->projMat = glm::ortho(-extents.x / 2.0f,
                extents.x / 2.0f,
                -extents.y / 2.0f,
                extents.y / 2.0f,
                clip[0],
                clip[1]);
            this->invProjMat = glm::inverse(projMat);
        }
    }

    void View::UpdateViewMatrix(Lock<Read<TransformSnapshot>> lock, Entity e) {
        if (e.Has<TransformSnapshot>(lock)) {
            this->invViewMat = e.Get<TransformSnapshot>(lock).globalPose.GetMatrix();
            this->viewMat = glm::inverse(this->invViewMat);
        }
    }
} // namespace ecs
