#include "Transform.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
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
        if (this->parent) {
            Assert(this->parent.Has<Transform>(lock), "cannot be relative to something that does not have a Transform");

            auto &parentTransform = this->parent.Get<Transform>(lock);

            if (this->cacheCount != this->changeCount || this->parentCacheCount != parentTransform.changeCount) {
                glm::mat4 parentModel = parentTransform.GetGlobalTransform(lock);
                this->cachedTransform = parentModel * this->translate * GetRotateMatrix() * this->scale;
                this->cacheCount = this->changeCount;
                this->parentCacheCount = parentTransform.changeCount;
            }
        } else if (this->cacheCount != this->changeCount) {
            this->cachedTransform = this->translate * GetRotateMatrix() * this->scale;
            this->cacheCount = this->changeCount;
        }
    }

    glm::mat4 Transform::GetGlobalTransform(Lock<Read<Transform>> lock) const {
        if (this->parent) {
            Assert(this->parent.Has<Transform>(lock), "cannot be relative to something that does not have a Transform");

            auto &parentTransform = this->parent.Get<Transform>(lock);

            if (this->cacheCount != this->changeCount || this->parentCacheCount != parentTransform.changeCount) {
                glm::mat4 parentModel = parentTransform.GetGlobalTransform(lock);
                return parentModel * this->translate * GetRotateMatrix() * this->scale;
            }
        } else if (this->cacheCount != this->changeCount) {
            return this->translate * GetRotateMatrix() * this->scale;
        }
        return this->cachedTransform;
    }

    glm::quat Transform::GetGlobalRotation(Lock<Read<Transform>> lock) const {
        glm::quat model = glm::identity<glm::quat>();

        if (this->parent) {
            Assert(this->parent.Has<Transform>(lock), "cannot be relative to something that does not have a Transform");
            model = this->parent.Get<Transform>(lock).GetGlobalRotation(lock);
        }

        return model * this->rotate;
    }

    glm::vec3 Transform::GetGlobalPosition(Lock<Read<Transform>> lock) const {
        return GetGlobalTransform(lock) * glm::vec4(0, 0, 0, 1);
    }

    glm::vec3 Transform::GetGlobalForward(Lock<Read<Transform>> lock) const {
        glm::vec3 forward = glm::vec3(0, 0, -1);
        return GetGlobalRotation(lock) * forward;
    }

    void Transform::Translate(glm::vec3 xyz) {
        this->translate = glm::translate(this->translate, xyz);
        this->changeCount++;
        this->dirty = true;
    }

    void Transform::Rotate(float radians, glm::vec3 axis) {
        this->rotate = glm::rotate(this->rotate, radians, axis);
        this->changeCount++;
        this->dirty = true;
    }

    void Transform::Scale(glm::vec3 xyz) {
        this->scale = glm::scale(this->scale, xyz);
        this->changeCount++;
        this->dirty = true;
    }

    void Transform::SetTranslate(glm::mat4 mat) {
        this->translate = mat;
        this->changeCount++;
        this->dirty = true;
    }

    glm::mat4 Transform::GetTranslate() const {
        return this->translate;
    }

    void Transform::SetPosition(glm::vec3 pos, bool setDirty) {
        this->translate = glm::column(this->translate, 3, glm::vec4(pos.x, pos.y, pos.z, 1.f));
        this->changeCount++;
        if (setDirty) this->dirty = true;
    }

    glm::vec3 Transform::GetPosition() const {
        return this->translate * glm::vec4(0, 0, 0, 1);
    }

    glm::vec3 Transform::GetUp() const {
        return GetRotate() * glm::vec3(0, 1, 0);
    }

    glm::vec3 Transform::GetForward() const {
        return GetRotate() * glm::vec3(0, 0, -1);
    }

    glm::vec3 Transform::GetLeft() const {
        return GetRotate() * glm::vec3(1, 0, 0);
    }

    glm::vec3 Transform::GetRight() const {
        return -GetLeft();
    }

    void Transform::SetRotate(glm::mat4 mat, bool setDirty) {
        this->rotate = mat;
        this->changeCount++;
        if (setDirty) this->dirty = true;
    }

    void Transform::SetRotate(glm::quat quat, bool setDirty) {
        this->rotate = quat;
        this->changeCount++;
        if (setDirty) this->dirty = true;
    }

    glm::quat Transform::GetRotate() const {
        return this->rotate;
    }

    glm::mat4 Transform::GetRotateMatrix() const {
        return glm::mat4_cast(rotate);
    }

    void Transform::SetScale(glm::vec3 xyz) {
        this->scale = glm::scale(glm::mat4(), xyz);
        this->changeCount++;
        this->dirty = true;
    }

    void Transform::SetScale(glm::mat4 mat) {
        this->scale = mat;
        this->changeCount++;
        this->dirty = true;
    }

    glm::mat4 Transform::GetScale() const {
        return this->scale;
    }

    glm::vec3 Transform::GetScaleVec() const {
        return this->scale * glm::vec4(1, 1, 1, 0);
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
