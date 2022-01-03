#pragma once

#include <ecs/CHelpers.h>

#ifdef __cplusplus
namespace ecs {
    extern "C" {
#endif

    typedef struct Transform {
        TecsEntity parent;
        GlmMat4x3 transform;
        uint32_t changeCount;

#ifdef __cplusplus
        Transform() : changeCount(1) {}
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

    // C accessors
    void transform_identity(Transform *out);
    void transform_from_pos(const GlmVec3 *pos, Transform *out);

    void transform_set_parent(Transform *t, TecsEntity ent);
    uint64_t transform_get_parent(const Transform *t);
    bool transform_has_parent(const Transform *t, ScriptLockHandle lock);

    void transform_get_global_mat4(const Transform *t, ScriptLockHandle lock, GlmMat4 *out);
    void transform_get_global_orientation(const Transform *t, ScriptLockHandle lock, GlmQuat *out);
    void transform_get_global_position(const Transform *t, ScriptLockHandle lock, GlmVec3 *out);
    void transform_get_global_forward(const Transform *t, ScriptLockHandle lock, GlmVec3 *out);

    void transform_translate(Transform *t, const GlmVec3 *xyz);
    void transform_rotate(Transform *t, float radians, const GlmVec3 *axis);
    void transform_scale(Transform *t, const GlmVec3 *xyz);

    void transform_set_position(Transform *t, const GlmVec3 *pos);
    void transform_set_rotation(Transform *t, const GlmQuat *quat);
    void transform_set_scale(Transform *t, const GlmVec3 *xyz);

    void transform_get_position(const Transform *t, GlmVec3 *out);
    void transform_get_rotation(const Transform *t, GlmQuat *out);
    void transform_get_scale(const Transform *t, GlmVec3 *out);

    uint32_t transform_change_number(const Transform *t);
    bool transform_has_changed(const Transform *t, uint32_t changeNumber);

#ifdef __cplusplus
    static_assert(sizeof(Transform) == 64, "Wrong Transform size");
    } // extern "C"

    static Component<Transform> ComponentTransform("transform");

    template<>
    bool Component<Transform>::Load(sp::Scene *scene, Transform &dst, const picojson::value &src);
} // namespace ecs
#endif
