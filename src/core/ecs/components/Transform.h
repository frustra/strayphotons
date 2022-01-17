#pragma once

#include <ecs/CHelpers.h>

#ifdef __cplusplus
namespace ecs {
    extern "C" {
#endif

    typedef struct Transform {
        GlmMat4x3 transform;
        TecsEntity parent;
        uint32_t changeCount;

#ifdef __cplusplus
    #ifndef SP_WASM_BUILD
        Transform() : changeCount(1) {}
        Transform(glm::mat4x3 transform) : transform(transform), changeCount(1) {}
        Transform(glm::vec3 pos, glm::quat orientation = glm::identity<glm::quat>());

        void SetParent(Tecs::Entity ent);
        const Tecs::Entity &GetParent() const;
        bool HasParent(Lock<Read<Transform>> lock) const;
        bool HasParent(Lock<Read<Transform>> lock, Tecs::Entity ent) const;

        // Returns a flattened Transform that includes all parent transforms.
        Transform GetGlobalTransform(Lock<Read<Transform>> lock) const;
        glm::quat GetGlobalRotation(Lock<Read<Transform>> lock) const;

        // Perform relative transform operations on the local transform data.
        void Translate(glm::vec3 xyz);
        void Rotate(float radians, glm::vec3 axis);
        void Scale(glm::vec3 xyz);

        // Sets local transform data, parent transforms are unaffected.
        void SetPosition(glm::vec3 pos);
        void SetRotation(glm::quat quat);
        void SetScale(glm::vec3 xyz);
        void SetTransform(glm::mat4x3 transform);
        void SetTransform(const Transform &transform);

        // Returns local transform data, not including any parent transforms.
        glm::vec3 GetPosition() const;
        glm::quat GetRotation() const;
        glm::vec3 GetForward() const;
        glm::vec3 GetScale() const;
        glm::mat4x3 GetMatrix() const;

        uint32_t ChangeNumber() const;
        bool HasChanged(uint32_t changeNumber) const;
    #endif
#endif
    } Transform;

    // C accessors
    void transform_identity(Transform *out);
    void transform_from_pos(Transform *out, const GlmVec3 *pos);

    void transform_set_parent(Transform *t, TecsEntity ent);
    uint64_t transform_get_parent(const Transform *t);
    bool transform_has_parent(const Transform *t, ScriptLockHandle lock);

    void transform_translate(Transform *t, const GlmVec3 *xyz);
    void transform_rotate(Transform *t, float radians, const GlmVec3 *axis);
    void transform_scale(Transform *t, const GlmVec3 *xyz);

    void transform_set_position(Transform *t, const GlmVec3 *pos);
    void transform_set_rotation(Transform *t, const GlmQuat *quat);
    void transform_set_scale(Transform *t, const GlmVec3 *xyz);

    void transform_get_position(GlmVec3 *out, const Transform *t);
    void transform_get_rotation(GlmQuat *out, const Transform *t);
    void transform_get_scale(GlmVec3 *out, const Transform *t);

    uint32_t transform_change_number(const Transform *t);
    bool transform_has_changed(const Transform *t, uint32_t changeNumber);

#ifdef __cplusplus
    // If this changes, make sure it is the same in Rust and WASM
    static_assert(sizeof(Transform) == 60, "Wrong Transform size");
    } // extern "C"

    #ifndef SP_WASM_BUILD
    static Component<Transform> ComponentTransform("transform");

    template<>
    bool Component<Transform>::Load(sp::Scene *scene, Transform &dst, const picojson::value &src);
    #endif
} // namespace ecs
#endif
