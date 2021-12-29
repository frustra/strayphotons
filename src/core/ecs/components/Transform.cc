#include "Transform.hh"

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

    Transform::Transform(glm::vec3 pos, glm::quat orientation)
        : transform(glm::column(glm::mat4x3(glm::mat3_cast(orientation)), 3, pos)) {}

    void Transform::SetParent(Tecs::Entity ent) {
        this->parent = ent;
        this->parentCacheCount = 0;
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

    void Transform::UpdateCachedTransform(Lock<Write<Transform>> lock) {
        if (IsCacheUpToDate(lock)) return;

        if (this->parent) {
            Assert(this->parent.Has<Transform>(lock), "Transform parent entity does not have a Transform");

            auto &parentTransform = this->parent.Get<Transform>(lock);
            parentTransform.UpdateCachedTransform(lock);
            this->cachedTransform = parentTransform.GetGlobalTransform(lock) * glm::mat4(this->transform);

            if (this->parentCacheCount != parentTransform.changeCount) this->changeCount++;
            this->parentCacheCount = parentTransform.changeCount;
        } else {
            this->cachedTransform = this->transform;
        }

        this->cacheCount = this->changeCount;
    }

    bool Transform::IsCacheUpToDate(Lock<Read<Transform>> lock) const {
        if (this->parent) {
            Assert(this->parent.Has<Transform>(lock), "Transform parent entity does not have a Transform");

            auto &parentTransform = this->parent.Get<Transform>(lock);
            if (this->parentCacheCount != parentTransform.changeCount || !parentTransform.IsCacheUpToDate(lock)) {
                return false;
            }
        }
        return this->cacheCount == this->changeCount;
    }

    glm::mat4 Transform::GetGlobalTransform(Lock<Read<Transform>> lock) const {
        if (IsCacheUpToDate(lock)) return this->cachedTransform;

        if (!this->parent) return this->transform;

        Assert(this->parent.Has<Transform>(lock), "Transform parent entity does not have a Transform");

        auto &parentTransform = this->parent.Get<Transform>(lock);
        return parentTransform.GetGlobalTransform(lock) * glm::mat4(this->transform);
    }

    glm::quat Transform::GetGlobalRotation(Lock<Read<Transform>> lock) const {
        glm::quat model = glm::identity<glm::quat>();

        if (this->parent) {
            Assert(this->parent.Has<Transform>(lock), "Transform parent entity does not have a Transform");
            model = this->parent.Get<Transform>(lock).GetGlobalRotation(lock);
        }

        return model * GetRotation();
    }

    glm::vec3 Transform::GetGlobalPosition(Lock<Read<Transform>> lock) const {
        return GetGlobalTransform(lock) * glm::vec4(0, 0, 0, 1);
    }

    glm::vec3 Transform::GetGlobalForward(Lock<Read<Transform>> lock) const {
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

    glm::vec3 Transform::GetUp() const {
        return glm::normalize(glm::mat3(this->transform) * glm::vec3(0, 1, 0));
    }

    glm::vec3 Transform::GetForward() const {
        return glm::normalize(glm::mat3(this->transform) * glm::vec3(0, 0, -1));
    }

    glm::vec3 Transform::GetLeft() const {
        return glm::normalize(glm::mat3(this->transform) * glm::vec3(1, 0, 0));
    }

    glm::vec3 Transform::GetRight() const {
        return -GetLeft();
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
        return glm::vec3(glm::length(this->transform[0]),
            glm::length(this->transform[1]),
            glm::length(this->transform[2]));
    }

    uint32_t Transform::ChangeNumber() const {
        return this->changeCount;
    }

    bool Transform::HasChanged(uint32_t changeNumber) const {
        return this->changeCount != changeNumber;
    }
} // namespace ecs
