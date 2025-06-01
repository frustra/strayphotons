/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
    #ifndef SP_WASM_BUILD
        #include <ecs/Components.hh>
        #include <ecs/Ecs.hh>
        #include <glm/glm.hpp>
        #include <glm/gtc/quaternion.hpp>
    #endif

extern "C" {
#else
typedef uint8_t bool;
#endif

#if defined(__cplusplus) && !defined(SP_WASM_BUILD)
typedef glm::vec3 GlmVec3;
typedef glm::quat GlmQuat;
typedef glm::mat4x3 GlmMat4x3;
typedef glm::mat4 GlmMat4;
#else
typedef float GlmVec3[3];
typedef float GlmQuat[4];
typedef float GlmMat4x3[4][3];
typedef float GlmMat4[4][4];
#endif

#ifdef __cplusplus
} // extern "C"
#endif
