#include "Transform.h"

#include "assets/JsonHelpers.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

namespace ecs {
    template<>
    bool StructMetadata::Load<Transform>(const EntityScope &scope, Transform &transform, const picojson::value &src) {
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
    void StructMetadata::Save<Transform>(const EntityScope &scope,
        picojson::value &dst,
        const Transform &src,
        const Transform &def) {
        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &obj = dst.get<picojson::object>();

        static const ecs::Transform defaultTransform = {};
        static const auto defaultRotation = defaultTransform.GetRotation();
        static const auto defaultScale = defaultTransform.GetScale();

        sp::json::SaveIfChanged(scope, obj, "translate", src.GetPosition(), defaultTransform.GetPosition());
        sp::json::SaveIfChanged(scope, obj, "rotate", src.GetRotation(), defaultRotation);

        auto scale = src.GetScale();
        if (glm::any(glm::epsilonNotEqual(scale, defaultScale, std::numeric_limits<float>::epsilon() * 5.0f))) {
            // If the scale is the same in all axes, only save a single float
            if (glm::all(glm::epsilonEqual(scale, glm::vec3(scale.x), std::numeric_limits<float>::epsilon() * 5.0f))) {
                sp::json::Save(scope, obj["scale"], scale.x);
            } else {
                sp::json::Save(scope, obj["scale"], scale);
            }
        }
    }

    template<>
    void StructMetadata::InitUndefined<TransformTree>(TransformTree &dst) {
        dst.pose.offset = glm::mat4x3(-INFINITY);
        dst.pose.scale = glm::vec3(-INFINITY);
    }

    template<>
    void Component<TransformTree>::Apply(TransformTree &dst, const TransformTree &src, bool liveTarget) {
        DebugAssert(!std::isnan(src.pose.offset[0][0]), "TransformTree::Apply source pose is NaN");
        auto &defaultTree = liveTarget ? ComponentTransformTree.defaultLiveComponent
                                       : ComponentTransformTree.defaultStagingComponent;

        if (dst.pose == defaultTree.pose && dst.parent == defaultTree.parent) {
            if (!std::isinf(src.pose.offset[0][0])) dst.pose = src.pose;
            dst.parent = src.parent;
        }
    }

    Transform::Transform(const glm::mat4 &matrix) : offset(matrix) {
        scale = glm::vec3(glm::length(offset[0]), glm::length(offset[1]), glm::length(offset[2]));
        offset[0] = glm::normalize(offset[0]);
        offset[1] = glm::normalize(offset[1]);
        offset[2] = glm::normalize(offset[2]);
    }

    Transform::Transform(glm::vec3 pos, glm::quat orientation)
        : offset(glm::column(glm::mat4x3(glm::mat3_cast(orientation)), 3, pos)), scale(glm::vec3(1.0f)) {}

    void initIfUndefined(Transform &transform) {
        if (std::isinf(transform.offset[0][0])) {
            transform.offset = glm::identity<glm::mat4x3>();
            transform.scale = glm::vec3(1.0f);
        }
    }

    void Transform::Translate(const glm::vec3 &xyz) {
        initIfUndefined(*this);
        offset[3] += xyz;
    }

    void Transform::Rotate(float radians, const glm::vec3 &axis) {
        initIfUndefined(*this);
        glm::mat3 &rotation = reinterpret_cast<glm::mat3 &>(offset);
        rotation = glm::rotate(glm::mat4(rotation), radians, axis);
    }

    void Transform::Rotate(const glm::quat &quat) {
        initIfUndefined(*this);
        reinterpret_cast<glm::mat3 &>(offset) *= glm::mat3_cast(quat);
    }

    void Transform::Scale(const glm::vec3 &xyz) {
        initIfUndefined(*this);
        scale *= xyz;
    }

    void Transform::SetPosition(const glm::vec3 &pos) {
        initIfUndefined(*this);
        offset[3] = pos;
    }

    const glm::vec3 &Transform::GetPosition() const {
        if (std::isinf(offset[0][0])) {
            static const glm::vec3 origin = {0, 0, 0};
            return origin;
        }
        DebugAssert(!std::isnan(offset[0][0]), "Transform pose is NaN");
        return offset[3];
    }

    void Transform::SetRotation(const glm::quat &quat) {
        initIfUndefined(*this);
        reinterpret_cast<glm::mat3 &>(offset) = glm::mat3_cast(quat);
    }

    glm::quat Transform::GetRotation() const {
        if (std::isinf(offset[0][0])) return glm::identity<glm::quat>();
        return glm::normalize(glm::quat_cast(reinterpret_cast<const glm::mat3 &>(offset)));
    }

    glm::vec3 Transform::GetForward() const {
        if (std::isinf(offset[0][0])) return glm::vec3(0, 0, -1);
        return glm::normalize(reinterpret_cast<const glm::mat3 &>(offset) * glm::vec3(0, 0, -1));
    }

    glm::vec3 Transform::GetUp() const {
        if (std::isinf(offset[0][0])) return glm::vec3(0, 1, 0);
        return glm::normalize(reinterpret_cast<const glm::mat3 &>(offset) * glm::vec3(0, 1, 0));
    }

    void Transform::SetScale(const glm::vec3 &xyz) {
        initIfUndefined(*this);
        scale = xyz;
    }

    const glm::vec3 &Transform::GetScale() const {
        if (std::isinf(offset[0][0])) {
            static const glm::vec3 noScale = {1, 1, 1};
            return noScale;
        }
        return scale;
    }

    const Transform &Transform::Get() const {
        if (std::isinf(offset[0][0])) {
            static const Transform identity = {};
            return identity;
        }
        DebugAssert(!std::isnan(offset[0][0]), "Transform pose is NaN");
        return *this;
    }

    Transform Transform::GetInverse() const {
        if (std::isinf(offset[0][0])) return Transform(glm::identity<glm::mat4x3>(), glm::vec3(1.0f));
        // return Transform(glm::affineInverse(GetMatrix()));

        ZoneScoped;
        // Optimized inverse taking advantage of separate scale and offset
        glm::mat3 inv = offset;
        inv[0] /= scale.x;
        inv[1] /= scale.y;
        inv[2] /= scale.z;
        inv = glm::transpose(inv);
        glm::mat4 newOffset = inv;
        newOffset[3] = glm::vec4(inv * -offset[3], 1.0f);
        return Transform(newOffset);
    }

    glm::mat4 Transform::GetMatrix() const {
        if (std::isinf(offset[0][0])) return glm::identity<glm::mat4>();
        return glm::mat4x3(offset[0] * scale.x, offset[1] * scale.y, offset[2] * scale.z, offset[3]);
    }

    glm::vec3 Transform::operator*(const glm::vec4 &rhs) const {
        return GetMatrix() * rhs;
    }

    Transform Transform::operator*(const Transform &rhs) const {
        return Transform(GetMatrix() * rhs.GetMatrix());
    }

    bool Transform::operator==(const Transform &other) const {
        return offset == other.offset && scale == other.scale;
    }

    bool Transform::operator!=(const Transform &other) const {
        return offset != other.offset || scale != other.scale;
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
