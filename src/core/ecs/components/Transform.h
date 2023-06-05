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
        GlmMat4x3 offset;
        GlmVec3 scale;

#ifdef __cplusplus
    #ifndef SP_WASM_BUILD
        Transform() : scale(1) {}
        Transform(const glm::mat4x3 &offset, glm::vec3 scale) : offset(offset), scale(scale) {}
        Transform(const glm::mat4 &matrix);
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
        const glm::vec3 &GetScale() const;
        const Transform &Get() const;
        Transform GetInverse() const;
        glm::mat4 GetMatrix() const;

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
    static_assert(sizeof(Transform) == 60, "Wrong Transform size");

    } // extern "C"

    #ifndef SP_WASM_BUILD

    static StructMetadata MetadataTransform(typeid(Transform),
        "Transform",
        "",
        StructField("translate",
            "Specifies the entity's position in 3D space. "
            "The +X direction represents Right, +Y represents Up, and -Z represents Forward.",
            typeid(glm::vec3),
            StructField::OffsetOf(&Transform::offset) + sizeof(glm::mat3),
            FieldAction::None),
        StructField("rotate",
            "Specifies the entity's orientation in 3D space. "
            "Multiple rotations can be combined by specifying an array of rotations: "
            "`[[90, 1, 0, 0], [-90, 0, 1, 0]]` is equivalent to `[120, 1, -1, -1]`. "
            "The rotation axis is automatically normalized.",
            typeid(glm::mat3),
            StructField::OffsetOf(&Transform::offset),
            FieldAction::None),
        StructField::New("scale",
            "Specifies the entity's size along each axis. A value of `[1, 1, 1]` leaves the size unchanged. "
            "If the scale is the same on all axes, a single scalar can be specified like `\"scale\": 0.5`",
            &Transform::scale,
            FieldAction::None));
    template<>
    bool StructMetadata::Load<Transform>(Transform &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<Transform>(const EntityScope &scope,
        picojson::value &dst,
        const Transform &src,
        const Transform *def);
    template<>
    void StructMetadata::DefineSchema<Transform>(picojson::value &dst, sp::json::SchemaTypeReferences *references);

    struct TransformSnapshot {
        Transform globalPose;

        TransformSnapshot() {}
        TransformSnapshot(const Transform &pose) : globalPose(pose) {}

        operator Transform() const {
            return globalPose;
        }
    };

    static StructMetadata MetadataTransformSnapshot(typeid(TransformSnapshot),
        "TransformSnapshot",
        R"(
Transform snapshots should not be set directly.
They are automatically generated for all entities with a `transform` component, and updated by the physics system.

This component stores a flattened version of an entity's transform tree. 
This represents and an entity's absolute position, orientation, and scale in the world relative to the origin.

Transform snapshots are used by the render thread for drawing entities in a physics-synchronized state,
while allowing multiple threads to independantly update entity transforms.
Snapshots are also useful for reading in scripts to reduce matrix multiplication costs and for similar sychronization benefits.
)",
        StructField::New(&TransformSnapshot::globalPose, FieldAction::AutoSave));
    static Component<TransformSnapshot> ComponentTransformSnapshot(MetadataTransformSnapshot, "transform_snapshot");

    struct TransformTree {
        Transform pose;
        EntityRef parent;

        TransformTree() {}
        TransformTree(const glm::mat4x3 &pose, glm::vec3 scale) : pose(pose, scale) {}
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

    static StructMetadata MetadataTransformTree(typeid(TransformTree),
        "TransformTree",
        R"(
Multiple entities with transforms can be linked together to create a tree of entities that all move together (i.e. a transform tree).

Transforms are combined in the following way:  
`scale -> rotate -> translate ( -> parent transform)`

Note: When combining multiple transformations together with scaling factors,
behavior is undefined if the combinations introduce skew. (The scale should be axis-aligned to the model)
)",
        StructField::New(&TransformTree::pose, ~FieldAction::AutoApply),
        StructField::New("parent",
            "Specifies a parent entity that this transform is relative to. "
            "If empty, the transform is relative to the scene root.",
            &TransformTree::parent,
            ~FieldAction::AutoApply));
    static Component<TransformTree> ComponentTransformTree(MetadataTransformTree, "transform");
    template<>
    void StructMetadata::InitUndefined<TransformTree>(TransformTree &dst);
    template<>
    void Component<TransformTree>::Apply(TransformTree &dst, const TransformTree &src, bool liveTarget);
    #endif
} // namespace ecs
#endif
