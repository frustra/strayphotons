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
    bool Component<Transform>::Load(sp::Scene *scene, Transform &transform, const picojson::value &src) {
        for (auto subTransform : src.get<picojson::object>()) {
            if (subTransform.first == "parent") {
                Assert(scene, "Transform::Load must have valid scene to define parent");
                auto parentName = subTransform.second.get<string>();
                auto it = scene->namedEntities.find(parentName);
                if (it != scene->namedEntities.end()) {
                    transform.SetParent(it->second);
                } else {
                    Errorf("Component<Transform>::Load parent name does not exist: %s", parentName);
                }
            } else if (subTransform.first == "scale") {
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

    Transform::Transform(glm::vec3 pos, glm::quat orientation)
        : transform(glm::column(glm::mat4x3(glm::mat3_cast(orientation)), 3, pos)), changeCount(1) {}

    void Transform::SetParent(Tecs::Entity ent) {
        this->parent = ent;
        this->changeCount++;
    }

    const Tecs::Entity &Transform::GetParent() const {
        return this->parent;
    }

    bool Transform::HasParent(Lock<Read<Transform>> lock) const {
        return this->parent && this->parent.Has<Transform>(lock);
    }

    bool Transform::HasParent(Lock<Read<Transform>> lock, Tecs::Entity ent) const {
        if (!HasParent(lock)) return false;
        return this->parent == ent || this->parent.Get<Transform>(lock).HasParent(lock, ent);
    }

    Transform Transform::GetGlobalTransform(Lock<Read<Transform>> lock) const {
        if (!this->parent) return *this;

        if (!this->parent.Has<Transform>(lock)) {
            Errorf("Transform parent %s does not have a Transform", std::to_string(this->parent));
            return *this;
        }

        auto parentTransform = this->parent.Get<Transform>(lock).GetGlobalTransform(lock);
        return Transform(parentTransform.transform * glm::mat4(this->transform));
    }

    glm::quat Transform::GetGlobalRotation(Lock<Read<Transform>> lock) const {
        if (!this->parent) return GetRotation();

        if (!this->parent.Has<Transform>(lock)) {
            Errorf("Transform parent %s does not have a Transform", std::to_string(this->parent));
            return GetRotation();
        }

        return this->parent.Get<Transform>(lock).GetGlobalRotation(lock) * GetRotation();
    }

    void Transform::Translate(glm::vec3 xyz) {
        this->transform[3] += xyz;
        this->changeCount++;
    }

    void Transform::Rotate(float radians, glm::vec3 axis) {
        glm::vec3 scale = GetScale();
        glm::mat3 rotation = glm::mat3(this->transform[0] / scale.x,
            this->transform[1] / scale.y,
            this->transform[2] / scale.z);
        rotation = glm::rotate(glm::mat4(rotation), radians, axis);
        this->transform[0] = rotation[0] * scale.x;
        this->transform[1] = rotation[1] * scale.y;
        this->transform[2] = rotation[2] * scale.z;
        this->changeCount++;
    }

    void Transform::Scale(glm::vec3 xyz) {
        this->transform[0] *= xyz.x;
        this->transform[1] *= xyz.y;
        this->transform[2] *= xyz.z;
        this->changeCount++;
    }

    void Transform::SetPosition(glm::vec3 pos) {
        this->transform[3] = pos;
        this->changeCount++;
    }

    glm::vec3 Transform::GetPosition() const {
        return this->transform[3];
    }

    void Transform::SetRotation(glm::quat quat) {
        glm::vec3 scale = GetScale();
        glm::mat3 rotation = glm::mat3_cast(quat);
        this->transform[0] = rotation[0] * scale.x;
        this->transform[1] = rotation[1] * scale.y;
        this->transform[2] = rotation[2] * scale.z;
        this->changeCount++;
    }

    glm::quat Transform::GetRotation() const {
        glm::mat3 rotation = glm::mat3(glm::normalize(this->transform[0]),
            glm::normalize(this->transform[1]),
            glm::normalize(this->transform[2]));
        return glm::quat_cast(rotation);
    }

    glm::vec3 Transform::GetForward() const {
        glm::mat3 scaledRotation = glm::mat3(this->transform[0], this->transform[1], this->transform[2]);
        return glm::normalize(scaledRotation * glm::vec3(0, 0, -1));
    }

    void Transform::SetScale(glm::vec3 xyz) {
        this->transform[0] = glm::normalize(this->transform[0]) * xyz.x;
        this->transform[1] = glm::normalize(this->transform[1]) * xyz.y;
        this->transform[2] = glm::normalize(this->transform[2]) * xyz.z;
        this->changeCount++;
    }

    glm::vec3 Transform::GetScale() const {
        return glm::vec3(glm::length(this->transform[0]),
            glm::length(this->transform[1]),
            glm::length(this->transform[2]));
    }

    void Transform::SetTransform(glm::mat4x3 transform) {
        this->transform = transform;
        this->changeCount++;
    }

    void Transform::SetTransform(const Transform &transform) {
        this->transform = transform.transform;
        this->changeCount++;
    }

    glm::mat4x3 Transform::GetMatrix() const {
        return this->transform;
    }

    uint32_t Transform::ChangeNumber() const {
        return this->changeCount;
    }

    bool Transform::HasChanged(uint32_t changeNumber) const {
        return this->changeCount != changeNumber;
    }

    void transform_identity(Transform *out) {
        *out = Transform();
    }
    void transform_from_pos(Transform *out, const GlmVec3 *pos) {
        *out = Transform(*pos);
    }

    void transform_set_parent(Transform *t, TecsEntity ent) {
        t->SetParent(ent);
    }
    uint64_t transform_get_parent(const Transform *t) {
        return *reinterpret_cast<const uint64_t *>(&t->GetParent());
    }
    bool transform_has_parent(const Transform *t, ScriptLockHandle lock) {
        return t->HasParent(*lock);
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

    uint32_t transform_change_number(const Transform *t) {
        return t->ChangeNumber();
    }
    bool transform_has_changed(const Transform *t, uint32_t changeNumber) {
        return t->HasChanged(changeNumber);
    }

} // namespace ecs
