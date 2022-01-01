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
    uint64_t ptr;

#ifdef __cplusplus
    operator ScriptLock() const {
        return *reinterpret_cast<const ScriptLock *>(ptr);
    }
#endif
} CLockHandle;

typedef struct TecsEntity {
    uint64_t id;

#ifdef __cplusplus
    TecsEntity(const Tecs::Entity &x) : id(x.id) {}
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

#pragma pack(pop)

#ifdef __cplusplus
} // extern "C"
#endif
