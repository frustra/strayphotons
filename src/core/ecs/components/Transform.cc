#include "Transform.h"

#include "assets/AssetHelpers.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Transform>::Load(ScenePtr scenePtr, Transform &transform, const picojson::value &src) {
        for (auto subTransform : src.get<picojson::object>()) {
            if (subTransform.first == "scale") {
                transform.Scale(sp::MakeVec3(subTransform.second));
            } else if (subTransform.first == "rotate") {
                vector<picojson::value *> rotations;
                picojson::array &subSecond = subTransform.second.get<picojson::array>();
                if (subSecond.at(0).is<picojson::array>()) {
                    // multiple rotations were given
                    for (picojson::value &r : subSecond) {
                        rotations.push_back(&r);
                    }
                } else {
                    // a single rotation was given
                    rotations.push_back(&subTransform.second);
                }

                for (picojson::value *r : rotations) {
                    auto n = sp::MakeVec4(*r);
                    transform.Rotate(glm::radians(n[0]), {n[1], n[2], n[3]});
                }
            } else if (subTransform.first == "translate") {
                transform.Translate(sp::MakeVec3(subTransform.second));
            }
        }
        return true;
    }

    template<>
    bool Component<TransformTree>::Load(ScenePtr scenePtr, TransformTree &transform, const picojson::value &src) {
        auto scene = scenePtr.lock();
        for (auto subTransform : src.get<picojson::object>()) {
            if (subTransform.first == "parent") {
                Assert(scene, "Transform::Load must have valid scene to define parent");
                auto parentName = subTransform.second.get<string>();
                transform.parent = scene->GetStagingEntity(parentName);
                if (!transform.parent) {
                    Errorf("Component<Transform>::Load parent name does not exist: %s", parentName);
                    return false;
                }
            }
        }
        return Component<Transform>::Load(scenePtr, transform.pose, src);
    }

    template<>
    void Component<TransformTree>::ApplyComponent(Lock<ReadAll> srcLock,
        Entity src,
        Lock<AddRemove> dstLock,
        Entity dst) {
        if (src.Has<TransformTree>(srcLock) && !dst.Has<TransformSnapshot, TransformTree>(dstLock)) {
            auto &srcTree = src.Get<TransformTree>(srcLock);
            auto &dstTree = dst.Get<TransformTree>(dstLock);

            // Map transform parent from staging id to live id
            if (srcTree.parent.Has<SceneInfo>(srcLock)) {
                auto &sceneInfo = srcTree.parent.Get<SceneInfo>(srcLock);
                dstTree.parent = sceneInfo.liveId;
            } else {
                dstTree.parent = Entity();
            }
            dstTree.pose = srcTree.pose;
            dst.Set<TransformSnapshot>(dstLock, srcTree.GetGlobalTransform(srcLock));
        }
    }

    template<>
    void Component<TransformTree>::Apply(const TransformTree &src, Lock<AddRemove> lock, Entity dst) {
        auto &dstTree = dst.Get<TransformTree>(lock);

        // Map transform parent from staging id to live id
        if (src.parent) {
            dstTree.parent = src.parent;
        } else {
            dstTree.parent = Entity();
        }
        dstTree.pose = src.pose;
        dst.Get<TransformSnapshot>(lock);
    }

    Transform::Transform(glm::vec3 pos, glm::quat orientation)
        : matrix(glm::column(glm::mat4x3(glm::mat3_cast(orientation)), 3, pos)) {}

    void Transform::Translate(const glm::vec3 &xyz) {
        matrix[3] += xyz;
    }

    void Transform::Rotate(float radians, const glm::vec3 &axis) {
        glm::vec3 scale = GetScale();
        glm::mat3 rotation = glm::mat3(matrix[0] / scale.x, matrix[1] / scale.y, matrix[2] / scale.z);
        rotation = glm::rotate(glm::mat4(rotation), radians, axis);
        matrix[0] = rotation[0] * scale.x;
        matrix[1] = rotation[1] * scale.y;
        matrix[2] = rotation[2] * scale.z;
    }

    void Transform::Scale(const glm::vec3 &xyz) {
        matrix[0] *= xyz.x;
        matrix[1] *= xyz.y;
        matrix[2] *= xyz.z;
    }

    void Transform::SetPosition(const glm::vec3 &pos) {
        matrix[3] = pos;
    }

    const glm::vec3 &Transform::GetPosition() const {
        return matrix[3];
    }

    void Transform::SetRotation(const glm::quat &quat) {
        glm::vec3 scale = GetScale();
        glm::mat3 rotation = glm::mat3_cast(quat);
        matrix[0] = rotation[0] * scale.x;
        matrix[1] = rotation[1] * scale.y;
        matrix[2] = rotation[2] * scale.z;
    }

    glm::quat Transform::GetRotation() const {
        glm::mat3 rotation = glm::mat3(glm::normalize(matrix[0]), glm::normalize(matrix[1]), glm::normalize(matrix[2]));
        return glm::quat_cast(rotation);
    }

    glm::vec3 Transform::GetForward() const {
        glm::mat3 scaledRotation = glm::mat3(matrix[0], matrix[1], matrix[2]);
        return glm::normalize(scaledRotation * glm::vec3(0, 0, -1));
    }

    void Transform::SetScale(const glm::vec3 &xyz) {
        matrix[0] = glm::normalize(matrix[0]) * xyz.x;
        matrix[1] = glm::normalize(matrix[1]) * xyz.y;
        matrix[2] = glm::normalize(matrix[2]) * xyz.z;
    }

    glm::vec3 Transform::GetScale() const {
        return glm::vec3(glm::length(matrix[0]), glm::length(matrix[1]), glm::length(matrix[2]));
    }

    Transform TransformTree::GetGlobalTransform(Lock<Read<TransformTree>> lock) const {
        if (!parent) return pose;

        if (!parent.Has<TransformTree>(lock)) {
            Errorf("TransformTree parent %s does not have a TransformTree", std::to_string(parent));
            return pose;
        }

        auto parentTransform = parent.Get<TransformTree>(lock).GetGlobalTransform(lock);
        return Transform(parentTransform.matrix * glm::mat4(pose.matrix));
    }

    glm::quat TransformTree::GetGlobalRotation(Lock<Read<TransformTree>> lock) const {
        if (!parent) return pose.GetRotation();

        if (!parent.Has<TransformTree>(lock)) {
            Errorf("TransformTree parent %s does not have a TransformTree", std::to_string(parent));
            return pose.GetRotation();
        }

        return parent.Get<TransformTree>(lock).GetGlobalRotation(lock) * pose.GetRotation();
    }

    void transform_identity(Transform *out) {
        *out = Transform();
    }
    void transform_from_pos(Transform *out, const GlmVec3 *pos) {
        *out = Transform(*pos);
    }

    void transform_translate(Transform *t, const GlmVec3 *xyz) {
        t->Translate(*xyz);
    }
    void transform_rotate(Transform *t, float radians, const GlmVec3 *axis) {
        t->Rotate(radians, *axis);
    }
    void transform_scale(Transform *t, const GlmVec3 *xyz) {
        t->Scale(*xyz);
    }

    void transform_set_position(Transform *t, const GlmVec3 *pos) {
        t->SetPosition(*pos);
    }
    void transform_set_rotation(Transform *t, const GlmQuat *quat) {
        t->SetRotation(*quat);
    }
    void transform_set_scale(Transform *t, const GlmVec3 *xyz) {
        t->SetScale(*xyz);
    }

    void transform_get_position(GlmVec3 *out, const Transform *t) {
        *out = t->GetPosition();
    }
    void transform_get_rotation(GlmQuat *out, const Transform *t) {
        *out = t->GetRotation();
    }
    void transform_get_scale(GlmVec3 *out, const Transform *t) {
        *out = t->GetScale();
    }

} // namespace ecs
