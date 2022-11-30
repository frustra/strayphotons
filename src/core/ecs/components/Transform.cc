#include "Transform.h"

#include "assets/JsonHelpers.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

namespace sp::json {
    template<>
    bool Load(const ecs::EntityScope &scope, ecs::Transform &transform, const picojson::value &src) {
        if (!src.is<picojson::object>()) {
            Errorf("Invalid transform: %s", src.to_str());
            return false;
        }

        for (auto &param : src.get<picojson::object>()) {
            if (param.first == "scale") {
                if (param.second.is<double>()) {
                    transform.SetScale(glm::vec3(param.second.get<double>()));
                } else {
                    glm::vec3 scale(1);
                    if (!sp::json::Load(scope, scale, param.second)) {
                        Errorf("Invalid transform scale: %s", param.second.to_str());
                        return false;
                    }
                    transform.SetScale(scale);
                }
            } else if (param.first == "rotate") {
                if (!param.second.is<picojson::array>()) {
                    Errorf("Invalid transform rotation: %s", param.second.to_str());
                    return false;
                }
                auto &paramSecond = param.second.get<picojson::array>();
                if (paramSecond.at(0).is<picojson::array>()) {
                    // Multiple rotations were given
                    glm::quat orientation = glm::identity<glm::quat>();
                    for (auto &r : paramSecond) {
                        glm::quat rotation;
                        if (!sp::json::Load(scope, rotation, r)) {
                            Errorf("Invalid transform rotation: %s", param.second.to_str());
                            return false;
                        }
                        orientation *= rotation;
                    }
                    transform.SetRotation(orientation);
                } else {
                    // A single rotation was given
                    glm::quat rotation;
                    if (!sp::json::Load(scope, rotation, param.second)) {
                        Errorf("Invalid transform rotation: %s", param.second.to_str());
                        return false;
                    }
                    transform.SetRotation(rotation);
                }
            } else if (param.first == "translate") {
                glm::vec3 translate(0);
                if (!sp::json::Load(scope, translate, param.second)) {
                    Errorf("Invalid transform translation: %s", param.second.to_str());
                    return false;
                }
                transform.SetPosition(translate);
            }
        }
        return true;
    }

    template<>
    void Save(const ecs::EntityScope &scope, picojson::value &dst, const ecs::Transform &src) {
        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &obj = dst.get<picojson::object>();

        static const ecs::Transform defaultTransform = {};
        static const auto defaultRotation = defaultTransform.GetRotation();
        static const auto defaultScale = defaultTransform.GetScale();

        SaveIfChanged(scope, obj, "translate", src.GetPosition(), defaultTransform.GetPosition());
        SaveIfChanged(scope, obj, "rotate", src.GetRotation(), defaultRotation);
        SaveIfChanged(scope, obj, "scale", src.GetScale(), defaultScale);
    }
} // namespace sp::json

namespace ecs {
    template<>
    void StructMetadata::InitUndefined<TransformTree>(TransformTree &dst) {
        dst.pose = glm::mat4x3(-INFINITY);
    }

    template<>
    void Component<TransformTree>::Apply(const TransformTree &src, Lock<AddRemove> lock, Entity dst) {
        DebugAssert(!std::isnan(src.pose.matrix[0][0]), "TransformTree::Apply source pose is NaN");
        auto &defaultTree = IsLive(lock) ? ComponentTransformTree.defaultLiveComponent
                                         : ComponentTransformTree.defaultStagingComponent;

        auto &dstTree = dst.Get<TransformTree>(lock);
        if (dstTree.pose == defaultTree.pose && dstTree.parent == defaultTree.parent) {
            if (!std::isinf(src.pose.matrix[0][0])) dstTree.pose = src.pose;
            dstTree.parent = src.parent;
        }
    }

    Transform::Transform(glm::vec3 pos, glm::quat orientation)
        : matrix(glm::column(glm::mat4x3(glm::mat3_cast(orientation)), 3, pos)) {}

    void Transform::Translate(const glm::vec3 &xyz) {
        if (std::isinf(matrix[0][0])) matrix = glm::identity<glm::mat4x3>();
        matrix[3] += xyz;
    }

    void Transform::Rotate(float radians, const glm::vec3 &axis) {
        if (std::isinf(matrix[0][0])) matrix = glm::identity<glm::mat4x3>();
        glm::vec3 scale = GetScale();
        glm::mat3 rotation = glm::mat3(matrix[0] / scale.x, matrix[1] / scale.y, matrix[2] / scale.z);
        rotation = glm::rotate(glm::mat4(rotation), radians, axis);
        matrix[0] = rotation[0] * scale.x;
        matrix[1] = rotation[1] * scale.y;
        matrix[2] = rotation[2] * scale.z;
    }

    void Transform::Rotate(const glm::quat &quat) {
        if (std::isinf(matrix[0][0])) matrix = glm::identity<glm::mat4x3>();
        glm::vec3 scale = GetScale();
        glm::mat3 rotation = glm::mat3(matrix[0] / scale.x, matrix[1] / scale.y, matrix[2] / scale.z);
        rotation *= glm::mat3_cast(quat);
        matrix[0] = rotation[0] * scale.x;
        matrix[1] = rotation[1] * scale.y;
        matrix[2] = rotation[2] * scale.z;
    }

    void Transform::Scale(const glm::vec3 &xyz) {
        if (std::isinf(matrix[0][0])) matrix = glm::identity<glm::mat4x3>();
        matrix[0] *= xyz.x;
        matrix[1] *= xyz.y;
        matrix[2] *= xyz.z;
    }

    void Transform::SetPosition(const glm::vec3 &pos) {
        if (std::isinf(matrix[0][0])) matrix = glm::identity<glm::mat4x3>();
        matrix[3] = pos;
    }

    const glm::vec3 &Transform::GetPosition() const {
        if (std::isinf(matrix[0][0])) {
            static const glm::vec3 origin = {0, 0, 0};
            return origin;
        }
        DebugAssert(!std::isnan(matrix[0][0]), "Transform pose is NaN");
        return matrix[3];
    }

    void Transform::SetRotation(const glm::quat &quat) {
        if (std::isinf(matrix[0][0])) matrix = glm::identity<glm::mat4x3>();
        glm::vec3 scale = GetScale();
        glm::mat3 rotation = glm::mat3_cast(quat);
        matrix[0] = rotation[0] * scale.x;
        matrix[1] = rotation[1] * scale.y;
        matrix[2] = rotation[2] * scale.z;
    }

    glm::quat Transform::GetRotation() const {
        if (std::isinf(matrix[0][0])) return glm::identity<glm::quat>();
        glm::mat3 rotation = glm::mat3(glm::normalize(matrix[0]), glm::normalize(matrix[1]), glm::normalize(matrix[2]));
        return glm::normalize(glm::quat_cast(rotation));
    }

    glm::vec3 Transform::GetForward() const {
        if (std::isinf(matrix[0][0])) return glm::vec3(0, 0, -1);
        glm::mat3 scaledRotation = glm::mat3(matrix[0], matrix[1], matrix[2]);
        return glm::normalize(scaledRotation * glm::vec3(0, 0, -1));
    }

    glm::vec3 Transform::GetUp() const {
        if (std::isinf(matrix[0][0])) return glm::vec3(0, 1, 0);
        glm::mat3 scaledRotation = glm::mat3(matrix[0], matrix[1], matrix[2]);
        return glm::normalize(scaledRotation * glm::vec3(0, 1, 0));
    }

    void Transform::SetScale(const glm::vec3 &xyz) {
        if (std::isinf(matrix[0][0])) matrix = glm::identity<glm::mat4x3>();
        matrix[0] = glm::normalize(matrix[0]) * xyz.x;
        matrix[1] = glm::normalize(matrix[1]) * xyz.y;
        matrix[2] = glm::normalize(matrix[2]) * xyz.z;
    }

    glm::vec3 Transform::GetScale() const {
        if (std::isinf(matrix[0][0])) return glm::vec3(1);
        return glm::vec3(glm::length(matrix[0]), glm::length(matrix[1]), glm::length(matrix[2]));
    }

    const Transform &Transform::Get() const {
        if (std::isinf(matrix[0][0])) {
            static const Transform identity = {};
            return identity;
        }
        DebugAssert(!std::isnan(matrix[0][0]), "Transform pose is NaN");
        return *this;
    }

    Transform Transform::GetInverse() const {
        if (std::isinf(matrix[0][0])) return glm::identity<glm::mat4x3>();
        return Transform(glm::inverse(glm::mat4(matrix)));
    }

    glm::vec3 Transform::operator*(const glm::vec4 &rhs) const {
        return matrix * rhs;
    }

    Transform Transform::operator*(const Transform &rhs) const {
        return Transform(matrix * glm::mat4(rhs.matrix));
    }

    bool Transform::operator==(const Transform &other) const {
        return matrix == other.matrix;
    }

    bool Transform::operator!=(const Transform &other) const {
        return matrix != other.matrix;
    }

    Entity TransformTree::GetRoot(Lock<Read<TransformTree>> lock, Entity entity) {
        if (!entity.Has<TransformTree>(lock)) return {};

        auto &tree = entity.Get<TransformTree>(lock);
        auto parent = tree.parent.Get(lock);
        if (!parent.Has<TransformTree>(lock)) {
            return entity;
        }

        return GetRoot(lock, parent);
    }

    void TransformTree::MoveViaRoot(Lock<Write<TransformTree>> lock, Entity entity, Transform target) {
        if (!entity.Has<TransformTree>(lock)) return;
        auto &entityTree = entity.Get<TransformTree>(lock);

        auto root = GetRoot(lock, entity);
        if (!root.Has<TransformTree>(lock)) return;
        auto &rootTree = root.Get<TransformTree>(lock);
        rootTree.pose = target * entityTree.GetRelativeTransform(lock, root).GetInverse();
    }

    Transform TransformTree::GetGlobalTransform(Lock<Read<TransformTree>> lock) const {
        if (!parent) return pose.Get();

        auto parentEntity = parent.Get(lock);
        if (!parentEntity.Has<TransformTree>(lock)) {
            Tracef("TransformTree parent %s does not have a TransformTree", std::to_string(parentEntity));
            return pose.Get();
        }

        auto parentTransform = parentEntity.Get<TransformTree>(lock).GetGlobalTransform(lock);
        return parentTransform * pose.Get();
    }

    glm::quat TransformTree::GetGlobalRotation(Lock<Read<TransformTree>> lock) const {
        if (!parent) return pose.GetRotation();

        auto parentEntity = parent.Get(lock);
        if (!parentEntity.Has<TransformTree>(lock)) {
            Tracef("TransformTree parent %s does not have a TransformTree", std::to_string(parentEntity));
            return pose.GetRotation();
        }

        return parentEntity.Get<TransformTree>(lock).GetGlobalRotation(lock) * pose.GetRotation();
    }

    Transform TransformTree::GetRelativeTransform(Lock<Read<TransformTree>> lock, const Entity &relative) const {
        if (parent == relative) {
            return pose.Get();
        } else if (!parent) {
            if (!relative.Has<TransformTree>(lock)) {
                Tracef("GetRelativeTransform relative %s does not have a TransformTree", std::to_string(relative));
                return pose.Get();
            }
            auto relativeTransform = relative.Get<TransformTree>(lock).GetGlobalTransform(lock);
            return relativeTransform.GetInverse() * pose.Get();
        }

        auto parentEntity = parent.Get(lock);
        if (!parentEntity.Has<TransformTree>(lock)) {
            Tracef("TransformTree parent %s does not have a TransformTree", std::to_string(parentEntity));
            return pose.Get();
        }

        auto relativeTransform = parentEntity.Get<TransformTree>(lock).GetRelativeTransform(lock, relative);
        return relativeTransform * pose.Get();
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
