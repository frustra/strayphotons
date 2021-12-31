#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
    #include <ecs/Components.hh>
    #include <ecs/Ecs.hh>
    #include <glm/glm.hpp>
    #include <glm/gtc/quaternion.hpp>

using ScriptLock = ecs::Lock<ecs::WriteAll>;

extern "C" {
#else
typedef unsigned int bool;
#endif

#pragma pack(push, 4)
typedef struct CLockHandle {
    const void *ptr;

#ifdef __cplusplus
    operator ScriptLock() const {
        return *static_cast<const ScriptLock *>(ptr);
    }
#endif
} CLockHandle;
typedef struct TecsEntity {
    uint64_t id;

#ifdef __cplusplus
    operator bool() const {
        return bool(Tecs::Entity(id));
    }
    operator const Tecs::Entity() const {
        return Tecs::Entity(id);
    }
    int operator=(const Tecs::Entity &x) {
        return id = x.id;
    }
#endif
} TecsEntity;
typedef struct GlmVec3 {
    float data[3];

#ifdef __cplusplus
    operator glm::vec3() const {
        return *reinterpret_cast<const glm::vec3 *>(&data);
    }
    glm::vec3 &operator=(const glm::vec3 &x) {
        return *reinterpret_cast<glm::vec3 *>(&data) = x;
    }
#endif
} GlmVec3;
typedef struct GlmQuat {
    float data[4];

#ifdef __cplusplus
    operator glm::quat() const {
        return *reinterpret_cast<const glm::quat *>(&data);
    }
    glm::quat &operator=(const glm::quat &x) {
        return *reinterpret_cast<glm::quat *>(&data) = x;
    }
#endif
} GlmQuat;
typedef struct GlmMat4x3 {
    float data[4][3];

#ifdef __cplusplus
    operator glm::mat4x3() const {
        return *reinterpret_cast<const glm::mat4x3 *>(&data);
    }
    operator glm::mat4() const {
        return glm::mat4(*reinterpret_cast<const glm::mat4x3 *>(&data));
    }
    glm::vec3 &operator[](const size_t &i) {
        return *reinterpret_cast<glm::vec3 *>(&data[i]);
    }
    const glm::vec3 &operator[](const size_t &i) const {
        return *reinterpret_cast<const glm::vec3 *>(&data[i]);
    }
    glm::mat4x3 &operator=(const glm::mat4x3 &x) {
        return *reinterpret_cast<glm::mat4x3 *>(&data) = x;
    }
#endif
} GlmMat4x3;
typedef struct GlmMat4 {
    float data[4][4];

#ifdef __cplusplus
    operator glm::mat4() const {
        return *reinterpret_cast<const glm::mat4 *>(&data);
    }
    glm::mat4 &operator=(const glm::mat4 &x) {
        return *reinterpret_cast<glm::mat4 *>(&data) = x;
    }
#endif
} GlmMat4;

typedef struct Transform {
    TecsEntity parent;
    GlmMat4x3 transform;
    uint32_t changeCount;

#ifdef __cplusplus
    Transform();
    Transform(glm::vec3 pos, glm::quat orientation = glm::identity<glm::quat>());

    void SetParent(Tecs::Entity ent);
    const Tecs::Entity &GetParent() const;
    bool HasParent(ecs::Lock<ecs::Read<ecs::Transform>> lock) const;
    bool HasParent(ecs::Lock<ecs::Read<ecs::Transform>> lock, Tecs::Entity ent) const;

    // Returns transform data that including all parent transforms.
    glm::mat4 GetGlobalTransform(ecs::Lock<ecs::Read<ecs::Transform>> lock) const;
    glm::quat GetGlobalRotation(ecs::Lock<ecs::Read<ecs::Transform>> lock) const;
    glm::vec3 GetGlobalPosition(ecs::Lock<ecs::Read<ecs::Transform>> lock) const;
    glm::vec3 GetGlobalForward(ecs::Lock<ecs::Read<ecs::Transform>> lock) const;

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
#pragma pack(pop)

void transform_identity(Transform *out);
void transform_from_pos(const GlmVec3 *pos, Transform *out);

void transform_set_parent(Transform *t, TecsEntity ent);
TecsEntity transform_get_parent(const Transform *t);
bool transform_has_parent(const Transform *t, CLockHandle lock);

void transform_get_global_mat4(const Transform *t, CLockHandle lock, GlmMat4 *out);
void transform_get_global_orientation(const Transform *t, CLockHandle lock, GlmQuat *out);
void transform_get_global_position(const Transform *t, CLockHandle lock, GlmVec3 *out);
void transform_get_global_forward(const Transform *t, CLockHandle lock, GlmVec3 *out);

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
static_assert(sizeof(Transform) == 60, "Wrong Transform size");
}

namespace ecs {
    static Component<Transform> ComponentTransform("transform");

    template<>
    bool Component<Transform>::Load(sp::Scene *scene, Transform &dst, const picojson::value &src);
} // namespace ecs
#endif
