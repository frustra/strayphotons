/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "core/Common.hh"

#include <foundation/PxQuat.h>
#include <foundation/PxTransform.h>
#include <foundation/PxVec3.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

inline static physx::PxVec3 GlmVec3ToPxVec3(const glm::vec3 &vector) {
    return physx::PxVec3(vector.x, vector.y, vector.z);
}

inline static glm::vec3 PxVec3ToGlmVec3(const physx::PxVec3 &vector) {
    return glm::vec3(vector.x, vector.y, vector.z);
}

inline static physx::PxExtendedVec3 GlmVec3ToPxExtendedVec3(const glm::vec3 &vector) {
    return physx::PxExtendedVec3(vector.x, vector.y, vector.z);
}

inline static glm::vec3 PxExtendedVec3ToGlmVec3(const physx::PxExtendedVec3 &vector) {
    return glm::vec3(vector.x, vector.y, vector.z);
}

inline static glm::quat PxQuatToGlmQuat(const physx::PxQuat &quat) {
    return glm::quat(quat.w, quat.x, quat.y, quat.z);
}

inline static physx::PxQuat GlmQuatToPxQuat(const glm::quat &quat) {
    return physx::PxQuat(quat.x, quat.y, quat.z, quat.w);
}

inline static glm::vec3 PxColorToGlmVec3(physx::PxU32 color) {
    uint8_t *parts = reinterpret_cast<uint8_t *>(&color);
    return glm::vec3(parts[2], parts[1], parts[0]) / 255.0f;
}

inline static glm::mat3 InertiaTensorMassToWorld(const glm::vec3 &massInertia, const glm::quat &orientation) {
    glm::mat3 M = glm::mat3_cast(orientation);
    glm::mat3 T = glm::matrixCompMult(glm::transpose(M), glm::mat3(massInertia, massInertia, massInertia));
    return M * T;
}
