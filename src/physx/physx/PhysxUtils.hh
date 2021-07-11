#pragma once

#include <foundation/PxMat44.h>
#include <foundation/PxTransform.h>
#include <foundation/PxVec3.h>
#include <geometry/PxMeshScale.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

inline static physx::PxVec3 GlmVec3ToPxVec3(const glm::vec3 vector) {
    return physx::PxVec3(vector.x, vector.y, vector.z);
}

inline static glm::vec3 PxVec3ToGlmVec3P(const physx::PxVec3 vector) {
    return glm::vec3(vector.x, vector.y, vector.z);
}

inline static physx::PxExtendedVec3 GlmVec3ToPxExtendedVec3(const glm::vec3 vector) {
    return physx::PxExtendedVec3(vector.x, vector.y, vector.z);
}

inline static glm::vec3 PxExtendedVec3ToGlmVec3P(const physx::PxExtendedVec3 vector) {
    return glm::vec3(vector.x, vector.y, vector.z);
}

inline static glm::quat PxQuatToGlmQuat(const physx::PxQuat quat) {
    return glm::quat(quat.w, quat.x, quat.y, quat.z);
}

inline static physx::PxQuat GlmQuatToPxQuat(const glm::quat quat) {
    return physx::PxQuat(quat.x, quat.y, quat.z, quat.w);
}

inline static physx::PxTransform GlmMat4ToPxTransform(const glm::mat4 mat) {
    // PxMat44 copies values out of the input array, so it is fine to cast away the const.
    return physx::PxTransform(physx::PxMat44(const_cast<float *>(glm::value_ptr(mat))));
}
