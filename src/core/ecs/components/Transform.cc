#include "Transform.h"

#include "assets/AssetHelpers.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Transform>::Load(sp::Scene *scene, Transform &transform, const picojson::value &src) {
        for (auto subTransform : src.get<picojson::object>()) {
            if (subTransform.first == "parent") {
                Assert(scene != nullptr, "Transform::Load must have valid scene to define parent");
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
} // namespace ecs

Transform::Transform() : parent(Tecs::Entity().id), changeCount(1) {
    transform = glm::identity<glm::mat4x3>();
}

Transform::Transform(glm::vec3 pos, glm::quat orientation) : parent(Tecs::Entity().id), changeCount(1) {
    transform = glm::column(glm::mat4x3(glm::mat3_cast(orientation)), 3, pos);
}

void Transform::SetParent(Tecs::Entity ent) {
    this->parent = ent.id;
    this->changeCount++;
}

const Tecs::Entity &Transform::GetParent() const {
    return *(Tecs::Entity *)&this->parent;
}

bool Transform::HasParent(ecs::Lock<ecs::Read<Transform>> lock) const {
    return this->parent && Tecs::Entity(this->parent).Has<Transform>(lock);
}

bool Transform::HasParent(ecs::Lock<ecs::Read<Transform>> lock, Tecs::Entity ent) const {
    if (!HasParent(lock)) return false;
    return this->parent.id == ent.id || Tecs::Entity(this->parent).Get<Transform>(lock).HasParent(lock, ent);
}

glm::mat4 Transform::GetGlobalTransform(ecs::Lock<ecs::Read<Transform>> lock) const {
    if (!this->parent) return this->transform;

    Assert(Tecs::Entity(this->parent).Has<Transform>(lock), "Transform parent entity does not have a Transform");

    auto &parentTransform = Tecs::Entity(this->parent).Get<Transform>(lock);
    return parentTransform.GetGlobalTransform(lock) * glm::mat4(this->transform);
}

glm::quat Transform::GetGlobalRotation(ecs::Lock<ecs::Read<Transform>> lock) const {
    glm::quat model = glm::identity<glm::quat>();

    if (Tecs::Entity(this->parent)) {
        Assert(Tecs::Entity(this->parent).Has<Transform>(lock), "Transform parent entity does not have a Transform");
        model = Tecs::Entity(this->parent).Get<Transform>(lock).GetGlobalRotation(lock);
    }

    return model * GetRotation();
}

glm::vec3 Transform::GetGlobalPosition(ecs::Lock<ecs::Read<Transform>> lock) const {
    return GetGlobalTransform(lock) * glm::vec4(0, 0, 0, 1);
}

glm::vec3 Transform::GetGlobalForward(ecs::Lock<ecs::Read<Transform>> lock) const {
    return GetGlobalRotation(lock) * glm::vec3(0, 0, -1);
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

void Transform::SetScale(glm::vec3 xyz) {
    this->transform[0] = glm::normalize(this->transform[0]) * xyz.x;
    this->transform[1] = glm::normalize(this->transform[1]) * xyz.y;
    this->transform[2] = glm::normalize(this->transform[2]) * xyz.z;
    this->changeCount++;
}

glm::vec3 Transform::GetScale() const {
    return glm::vec3(glm::length(this->transform[0]), glm::length(this->transform[1]), glm::length(this->transform[2]));
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
void transform_from_pos(const GlmVec3 *pos, Transform *out) {
    *out = Transform(*pos);
}

void transform_set_parent(Transform *t, TecsEntity ent) {
    t->SetParent(ent);
}
TecsEntity transform_get_parent(const Transform *t) {
    return TecsEntity{t->GetParent().id};
}
bool transform_has_parent(const Transform *t, CLockHandle lock) {
    return t->HasParent(ScriptLock(lock));
}

void transform_get_global_mat4(const Transform *t, CLockHandle lock, GlmMat4 *out) {
    *out = t->GetGlobalTransform(ScriptLock(lock));
}
void transform_get_global_orientation(const Transform *t, CLockHandle lock, GlmQuat *out) {
    *out = t->GetGlobalRotation(ScriptLock(lock));
}
void transform_get_global_position(const Transform *t, CLockHandle lock, GlmVec3 *out) {
    *out = t->GetGlobalPosition(ScriptLock(lock));
}
void transform_get_global_forward(const Transform *t, CLockHandle lock, GlmVec3 *out) {
    *out = t->GetGlobalForward(ScriptLock(lock));
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

void transform_get_position(const Transform *t, GlmVec3 *out) {
    *out = t->GetPosition();
}
void transform_get_rotation(const Transform *t, GlmQuat *out) {
    *out = t->GetRotation();
}
void transform_get_scale(const Transform *t, GlmVec3 *out) {
    *out = t->GetScale();
}

uint32_t transform_change_number(const Transform *t) {
    return t->ChangeNumber();
}
bool transform_has_changed(const Transform *t, uint32_t changeNumber) {
    return t->HasChanged(changeNumber);
}
