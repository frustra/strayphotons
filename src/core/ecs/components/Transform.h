#pragma once

#include <stdint.h>

#ifdef __cplusplus
    #include <ecs/Components.hh>
    #include <ecs/Ecs.hh>
    #include <glm/glm.hpp>
    #include <glm/gtc/quaternion.hpp>

using LockHandle = ecs::Lock<ecs::WriteAll> *;
using TecsEntity = Tecs::Entity;
using GlmVec3 = glm::vec3;
using GlmQuat = glm::quat;
using GlmMat4x3 = glm::mat4x3;
using GlmMat4 = glm::mat4;

namespace ecs {
    extern "C" {
#else
typedef const void *LockHandle;
typedef size_t TecsEntity;
typedef float GlmVec3[3];
typedef float GlmQuat[4];
typedef float GlmMat4x3[12];
typedef float GlmMat4[16];
#endif

    typedef struct Transform {
        TecsEntity parent;
        GlmMat4x3 transform = {
            // clang-format off
            // 1 line = 1 matrix columns
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            // clang-format on
        };
        uint32_t changeCount = 1;

#ifdef __cplusplus
        Transform() {}
        Transform(glm::vec3 pos, glm::quat orientation = glm::identity<glm::quat>());

        void SetParent(Tecs::Entity ent);
        const Tecs::Entity &GetParent() const;
        bool HasParent(Lock<Read<Transform>> lock) const;
        bool HasParent(Lock<Read<Transform>> lock, Tecs::Entity ent) const;

        // Returns transform data that including all parent transforms.
        glm::mat4 GetGlobalTransform(Lock<Read<Transform>> lock) const;
        glm::quat GetGlobalRotation(Lock<Read<Transform>> lock) const;
        glm::vec3 GetGlobalPosition(Lock<Read<Transform>> lock) const;
        glm::vec3 GetGlobalForward(Lock<Read<Transform>> lock) const;

        // Perform relative transform operations on the local transform data.
        void Translate(glm::vec3 xyz);
        void Rotate(float radians, glm::vec3 axis);
        void Scale(glm::vec3 xyz);

        // Sets local transform data, parent transforms are unaffected.
        void SetPosition(glm::vec3 pos);
        void SetRotation(glm::quat quat);
        void SetScale(glm::vec3 xyz);

        // Returns local transform data, not including any parent transforms.
        glm::vec3 GetPosition() const;
        glm::quat GetRotation() const;
        glm::vec3 GetScale() const;

        uint32_t ChangeNumber() const;
        bool HasChanged(uint32_t changeNumber) const;
#endif
    } Transform;

#ifdef __cplusplus
    }

    static Component<Transform> ComponentTransform("transform");

    template<>
    bool Component<Transform>::Load(sp::Scene *scene, Transform &dst, const picojson::value &src);
}
namespace {
    using Transform = ecs::Transform;
#endif

    void transform_set_parent(Transform *t, TecsEntity ent);
    TecsEntity transform_get_parent(const Transform *t);
    bool transform_has_parent(const Transform *t, LockHandle lock, TecsEntity ent = ~(size_t)0);

    void transform_get_global_mat4(const Transform *t, LockHandle lock, GlmMat4 *out);
    void transform_get_global_orientation(const Transform *t, LockHandle lock, GlmQuat *out);
    void transform_get_global_position(const Transform *t, LockHandle lock, GlmVec3 *out);
    void transform_get_global_forward(const Transform *t, LockHandle lock, GlmVec3 *out);

    void transform_translate(Transform *t, GlmVec3 xyz);
    void transform_rotate(Transform *t, float radians, GlmVec3 axis);
    void transform_scale(Transform *t, GlmVec3 xyz);

    void transform_set_position(Transform *t, GlmVec3 pos);
    void transform_set_rotation(Transform *t, GlmQuat quat);
    void transform_set_scale(Transform *t, GlmVec3 xyz);

    void transform_get_position(const Transform *t, GlmVec3 *out);
    void transform_get_rotation(const Transform *t, GlmQuat *out);
    void transform_get_scale(const Transform *t, GlmVec3 *out);

    uint32_t transform_change_number(const Transform *t);
    bool transform_has_changed(const Transform *t, uint32_t changeNumber);

#ifdef __cplusplus
}
#endif
