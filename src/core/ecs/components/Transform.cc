#include "Transform.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Transform>::Load(Lock<Read<ecs::Name>> lock, Transform &transform, const picojson::value &src) {
        for (auto subTransform : src.get<picojson::object>()) {
            if (subTransform.first == "parent") {
                auto parentName = subTransform.second.get<string>();
                for (auto ent : lock.EntitiesWith<Name>()) {
                    if (ent.Get<Name>(lock) == parentName) {
                        transform.SetParent(ent);
                        break;
                    }
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

    void Transform::SetParent(Tecs::Entity ent) {
        this->parent = ent;
        this->parentCacheCount = 0;
        this->dirty = true;
    }

    bool Transform::HasParent(Lock<Read<Transform>> lock) const {
        return this->parent && this->parent.Has<Transform>(lock);
    }

    void Transform::UpdateCachedTransform(Lock<Write<Transform>> lock) {
        if (IsCacheUpToDate(lock)) return;

        glm::mat4 transform = glm::mat3_cast(this->rotation) * glm::mat3(glm::scale(this->scale));
        transform = glm::column(transform, 3, glm::vec4(this->position, 1.0f));

        if (this->parent) {
            Assert(this->parent.Has<Transform>(lock), "Transform parent entity does not have a Transform");

            auto &parentTransform = this->parent.Get<Transform>(lock);
            parentTransform.UpdateCachedTransform(lock);
            transform = parentTransform.GetGlobalTransform(lock) * transform;

            if (this->parentCacheCount != parentTransform.changeCount) this->changeCount++;
            this->parentCacheCount = parentTransform.changeCount;
        }

        this->cachedTransform = transform;
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

        glm::mat4 transform = glm::mat3_cast(this->rotation) * glm::mat3(glm::scale(this->scale));
        transform = glm::column(transform, 3, glm::vec4(this->position, 1.0f));

        if (this->parent) {
            Assert(this->parent.Has<Transform>(lock), "Transform parent entity does not have a Transform");

            auto &parentTransform = this->parent.Get<Transform>(lock);
            return parentTransform.GetGlobalTransform(lock) * transform;
        }
        return transform;
    }

    glm::quat Transform::GetGlobalRotation(Lock<Read<Transform>> lock) const {
        glm::quat model = glm::identity<glm::quat>();

        if (this->parent) {
            Assert(this->parent.Has<Transform>(lock), "Transform parent entity does not have a Transform");
            model = this->parent.Get<Transform>(lock).GetGlobalRotation(lock);
        }

        return model * this->rotation;
    }

    glm::vec3 Transform::GetGlobalPosition(Lock<Read<Transform>> lock) const {
        return GetGlobalTransform(lock) * glm::vec4(0, 0, 0, 1);
    }

    glm::vec3 Transform::GetGlobalForward(Lock<Read<Transform>> lock) const {
        return GetGlobalRotation(lock) * glm::vec3(0, 0, -1);
    }

    void Transform::Translate(glm::vec3 xyz) {
        this->position += xyz;
        this->changeCount++;
        this->dirty = true;
    }

    void Transform::Rotate(float radians, glm::vec3 axis) {
        this->rotation = glm::rotate(this->rotation, radians, axis);
        this->changeCount++;
        this->dirty = true;
    }

    void Transform::Scale(glm::vec3 xyz) {
        this->scale *= xyz;
        this->changeCount++;
        this->dirty = true;
    }

    void Transform::SetPosition(glm::vec3 pos, bool setDirty) {
        this->position = pos;
        this->changeCount++;
        if (setDirty) this->dirty = true;
    }

    glm::vec3 Transform::GetPosition() const {
        return this->position;
    }

    glm::vec3 Transform::GetUp() const {
        return this->rotation * glm::vec3(0, 1, 0);
    }

    glm::vec3 Transform::GetForward() const {
        return this->rotation * glm::vec3(0, 0, -1);
    }

    glm::vec3 Transform::GetLeft() const {
        return this->rotation * glm::vec3(1, 0, 0);
    }

    glm::vec3 Transform::GetRight() const {
        return -GetLeft();
    }

    void Transform::SetRotation(glm::quat quat, bool setDirty) {
        this->rotation = quat;
        this->changeCount++;
        if (setDirty) this->dirty = true;
    }

    glm::quat Transform::GetRotation() const {
        return this->rotation;
    }

    void Transform::SetScale(glm::vec3 xyz) {
        this->scale = xyz;
        this->changeCount++;
        this->dirty = true;
    }

    glm::vec3 Transform::GetScale() const {
        return this->scale;
    }

    bool Transform::IsDirty() const {
        return this->dirty;
    }

    bool Transform::ClearDirty() {
        bool tmp = this->dirty;
        this->dirty = false;
        return tmp;
    }
} // namespace ecs
