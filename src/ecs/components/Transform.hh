#pragma once

#include "Common.hh"

#include <ecs/Components.hh>
#include <ecs/Ecs.hh>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace ecs {
    class Transform {
    public:
        Transform() : dirty(false) {}
        Transform(glm::vec3 pos, glm::quat orientation = glm::identity<glm::quat>())
            : rotate(orientation), dirty(false) {
            SetPosition(pos);
        }

        void SetParent(Tecs::Entity ent);
        bool HasParent(Lock<Read<Transform>> lock) const;
        void UpdateCachedTransform(Lock<Write<Transform>> lock);

        /**
         * Return the transformation matrix including all parent transforms.
         */
        glm::mat4 GetGlobalTransform(Lock<Read<Transform>> lock) const;

        /**
         * Returns the same as GetGlobalTransform() but only includes rotation components.
         */
        glm::quat GetGlobalRotation(Lock<Read<Transform>> lock) const;

        /**
         * Get position including any transforms this is relative to
         */
        glm::vec3 GetGlobalPosition(Lock<Read<Transform>> lock) const;

        /**
         * Get forward vector including any transforms this is relative to
         */
        glm::vec3 GetGlobalForward(Lock<Read<Transform>> lock) const;

        /**
         * Change the local position by an amount in the local x, y, z planes
         */
        void Translate(glm::vec3 xyz);

        /**
         * Change the local rotation by "radians" amount about the local "axis"
         */
        void Rotate(float radians, glm::vec3 axis);

        /**
         * Change the local scale by an amount in the local x, y, z planes.
         * This will propagate to Transforms that are relative to this one.
         */
        void Scale(glm::vec3 xyz);

        void SetTranslate(glm::mat4 mat);
        glm::mat4 GetTranslate() const;
        void SetPosition(glm::vec3 pos);
        glm::vec3 GetPosition() const;

        glm::vec3 GetUp() const;
        glm::vec3 GetForward() const;
        glm::vec3 GetLeft() const;
        glm::vec3 GetRight() const;

        void SetRotate(glm::mat4 mat);
        void SetRotate(glm::quat quat);
        glm::quat GetRotate() const;
        glm::mat4 GetRotateMatrix() const;

        void SetScale(glm::mat4 mat);
        void SetScale(glm::vec3 xyz);
        glm::mat4 GetScale() const;
        glm::vec3 GetScaleVec() const;

        bool ClearDirty();

        bool operator==(const Transform &other) const {
            return parent == other.parent && translate == other.translate && scale == other.scale &&
                   rotate == other.rotate;
        }

        bool operator!=(const Transform &other) const {
            return !(*this == other);
        }

    private:
        Tecs::Entity parent;

        glm::mat4 translate = glm::identity<glm::mat4>();
        glm::mat4 scale = glm::identity<glm::mat4>();
        glm::quat rotate = glm::identity<glm::quat>();

        glm::mat4 cachedTransform = glm::identity<glm::mat4>();
        uint32 changeCount = 1;
        uint32 cacheCount = 0;
        uint32 parentCacheCount = 0;

        bool dirty;

        template<typename>
        friend class Component;
    };

    static Component<Transform> ComponentTransform("transform");

    template<>
    bool Component<Transform>::Load(Lock<Read<ecs::Name>> lock, Transform &dst, const picojson::value &src);
    template<>
    bool Component<Transform>::Save(picojson::value &dst, const Transform &src);
} // namespace ecs
