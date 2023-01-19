#pragma once

#include <ecs/CHelpers.h>

#ifdef __cplusplus
    #ifndef SP_WASM_BUILD
        #include "ecs/Components.hh"
        #include "ecs/EntityRef.hh"
    #endif

namespace ecs {
    extern "C" {
#endif

    typedef struct Transform {
        GlmMat4x3 matrix;

#ifdef __cplusplus
    #ifndef SP_WASM_BUILD
        Transform() {}
        Transform(const glm::mat4x3 &matrix) : matrix(matrix) {}
        Transform(glm::vec3 pos, glm::quat orientation = glm::identity<glm::quat>());

        void Translate(const glm::vec3 &xyz);
        void Rotate(float radians, const glm::vec3 &axis);
        void Rotate(const glm::quat &quat);
        void Scale(const glm::vec3 &xyz);

        void SetPosition(const glm::vec3 &pos);
        void SetRotation(const glm::quat &quat);
        void SetScale(const glm::vec3 &xyz);

        const glm::vec3 &GetPosition() const;
        glm::quat GetRotation() const;
        glm::vec3 GetForward() const;
        glm::vec3 GetUp() const;
        glm::vec3 GetRight() const;
        glm::vec3 GetScale() const;
        const Transform &Get() const;
        Transform GetInverse() const;

        glm::vec3 operator*(const glm::vec4 &rhs) const;
        Transform operator*(const Transform &rhs) const;
        bool operator==(const Transform &other) const;
        bool operator!=(const Transform &other) const;

    #endif
#endif
    } Transform;

    // C accessors
    void transform_identity(Transform *out);
    void transform_from_pos(Transform *out, const GlmVec3 *pos);

    void transform_translate(Transform *t, const GlmVec3 *xyz);
    void transform_rotate(Transform *t, float radians, const GlmVec3 *axis);
    void transform_scale(Transform *t, const GlmVec3 *xyz);

    void transform_set_position(Transform *t, const GlmVec3 *pos);
    void transform_set_rotation(Transform *t, const GlmQuat *quat);
    void transform_set_scale(Transform *t, const GlmVec3 *xyz);

    void transform_get_position(GlmVec3 *out, const Transform *t);
    void transform_get_rotation(GlmQuat *out, const Transform *t);
    void transform_get_scale(GlmVec3 *out, const Transform *t);

#ifdef __cplusplus
    // If this changes, make sure it is the same in Rust and WASM
    static_assert(sizeof(Transform) == 48, "Wrong Transform size");

    #ifndef SP_WASM_BUILD
    typedef Transform TransformSnapshot;

    struct TransformTree {
        Transform pose;
        EntityRef parent;

        TransformTree() {}
        TransformTree(const glm::mat4x3 &pose) : pose(pose) {}
        TransformTree(const Transform &pose, EntityRef parent = {}) : pose(pose), parent(parent) {}
        TransformTree(glm::vec3 pos, glm::quat orientation = glm::identity<glm::quat>()) : pose(pos, orientation) {}

        static void MoveViaRoot(Lock<Write<TransformTree>> lock, Entity entity, Transform target);
        static Entity GetRoot(Lock<Read<TransformTree>> lock, Entity entity);

        // Returns a flattened Transform that includes all parent transforms.
        Transform GetGlobalTransform(Lock<Read<TransformTree>> lock) const;
        glm::quat GetGlobalRotation(Lock<Read<TransformTree>> lock) const;

        // Returns a flatted Transform relative to the specified entity.
        Transform GetRelativeTransform(Lock<Read<TransformTree>> lock, const Entity &relative) const;
    };
    #endif
    } // extern "C"

    #ifndef SP_WASM_BUILD
    static StructMetadata MetadataTransform(typeid(Transform));
    template<>
    bool StructMetadata::Load<Transform>(const EntityScope &scope, Transform &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<Transform>(const EntityScope &scope,
        picojson::value &dst,
        const Transform &src,
        const Transform &def);

    static StructMetadata MetadataTransformTree(typeid(TransformTree),
        StructField::New(&TransformTree::pose, ~FieldAction::AutoApply),
        StructField::New("parent", &TransformTree::parent, ~FieldAction::AutoApply));
    static Component<TransformTree> ComponentTransformTree("transform", MetadataTransformTree);
    static StructMetadata MetadataTransformSnapshot(typeid(TransformSnapshot),
        StructField::New<Transform>(FieldAction::AutoSave));
    static Component<TransformSnapshot> ComponentTransformSnapshot("transform_snapshot", MetadataTransformSnapshot);

    template<>
    void StructMetadata::InitUndefined<TransformTree>(TransformTree &dst);
    template<>
    void Component<TransformTree>::Apply(TransformTree &dst, const TransformTree &src, bool liveTarget);
    #endif
} // namespace ecs
#endif
