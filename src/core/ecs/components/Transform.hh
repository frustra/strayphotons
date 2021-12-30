#pragma once

#include <ecs/Components.hh>
#include <ecs/Ecs.hh>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace ecs {
    class Transform {
    public:
        Transform() {}
        Transform(glm::vec3 pos, glm::quat orientation = glm::identity<glm::quat>());

        void SetParent(Tecs::Entity ent);
        const Tecs::Entity &GetParent() const;
        bool HasParent(Lock<Read<Transform>> lock) const;
        bool HasParent(Lock<Read<Transform>> lock, Tecs::Entity ent) const;

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

        void SetPosition(glm::vec3 pos);
        glm::vec3 GetPosition() const;

        glm::vec3 GetUp() const;
        glm::vec3 GetForward() const;
        glm::vec3 GetLeft() const;
        glm::vec3 GetRight() const;

        void SetRotation(glm::quat quat);
        glm::quat GetRotation() const;

        void SetScale(glm::vec3 xyz);
        glm::vec3 GetScale() const;

        uint32_t ChangeNumber() const;
        bool HasChanged(uint32_t changeNumber) const;

    private:
        Tecs::Entity parent;
        glm::mat4x3 transform = glm::identity<glm::mat4x3>();
        uint32_t changeCount = 1;
    };

    static Component<Transform> ComponentTransform("transform");

    template<>
    bool Component<Transform>::Load(sp::Scene *scene, Transform &dst, const picojson::value &src);
} // namespace ecs
