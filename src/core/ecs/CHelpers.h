#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
    #include <ecs/Components.hh>
    #include <ecs/Ecs.hh>
    #include <glm/glm.hpp>
    #include <glm/gtc/quaternion.hpp>

extern "C" {
#else
typedef unsigned int bool;
#endif

#ifdef __cplusplus
typedef const ecs::Lock<ecs::WriteAll> *ScriptLockHandle;
typedef Tecs::Entity TecsEntity;
typedef glm::vec3 GlmVec3;
typedef glm::quat GlmQuat;
typedef glm::mat4x3 GlmMat4x3;
typedef glm::mat4 GlmMat4;
#else
typedef uint64_t ScriptLockHandle;
typedef uint64_t TecsEntity;
typedef float GlmVec3[3];
typedef float GlmQuat[4];
typedef float GlmMat4x3[4][3];
typedef float GlmMat4[4][4];
#endif

#ifdef __cplusplus
} // extern "C"
#endif
