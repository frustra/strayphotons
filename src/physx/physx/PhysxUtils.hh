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

inline static glm::mat3 InertiaTensorMassToWorld(const glm::vec3 &massInertia, const glm::quat &orientation) {
    glm::mat3 M = glm::mat3_cast(orientation);
    glm::mat3 T = glm::matrixCompMult(glm::transpose(M), glm::mat3(massInertia, massInertia, massInertia));
    return M * T;
}
