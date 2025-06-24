/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <c_abi/Tecs.h>
#include <c_abi/strayphotons_ecs_c_abi_entity_gen.h>
#include <c_abi/strayphotons_ecs_c_abi_lock_gen.h>
#include <cglm/cglm.h>
#include <stdbool.h>
#include <strayphotons/components.h>
#include <strayphotons/logging.h>
#include <string.h>

#ifdef SP_EXPORT
    #undef SP_EXPORT
#endif
#ifdef _WIN32
    #define SP_EXPORT __declspec(dllexport)
#else
    #define SP_EXPORT __attribute__((__visibility__("default")))
#endif

typedef struct script_camera_view_t {
    int foobar;
    bool started;
} script_camera_view_t;

SP_EXPORT void camera_view_init(void *context, sp_script_state_t *state) {
    script_camera_view_t *ctx = context;
    ctx->foobar = 42;
    ctx->started = false;
}

SP_EXPORT void camera_view_on_tick(void *context,
    sp_script_state_t *state,
    tecs_lock_t *lock,
    tecs_entity_t ent,
    uint64_t intervalNs) {
    script_camera_view_t *ctx = context;
    if (!Tecs_entity_has_transform_tree(lock, ent)) return;

    if (!ctx->started) {
        char buffer[256] = {0};
        snprintf(buffer, 255, "Script: %s = %d\n", sp_string_get_c_str(&state->definition.name), ctx->foobar);
        sp_log_message(SP_LOG_LEVEL_LOG, buffer);
        ctx->started = true;
    }

    sp_event_t *event;
    while ((event = sp_script_state_poll_event(state, lock))) {
        if (sp_string_compare(&event->name, "/script/camera_rotate") != 0) continue;
        if (sp_event_data_get_type(&event->data) != SP_EVENT_DATA_TYPE_VEC2) continue;

        const vec2_t *angleDiff = sp_event_data_get_const_vec2(&event->data);
        // Apply pitch/yaw rotations
        sp_ecs_transform_tree_t *transform = Tecs_entity_get_transform_tree(lock, ent);

        // char buffer[256] = {0};
        // snprintf(buffer, 255, "[camera_view] Mouse input: (%.3f, %.3f)\n", angleDiff->v[0], angleDiff->v[1]);
        // sp_log_message(SP_LOG_LEVEL_LOG, buffer);

        quat_t originalRotation;
        sp_transform_get_rotation(&transform->transform, &originalRotation);
        quat_t rotateY, rotateX;
        glm_quatv(rotateY.q, -angleDiff->v[0] * 1.0, (vec3){0, 1, 0});
        glm_quatv(rotateX.q, -angleDiff->v[1] * 1.0, (vec3){1, 0, 0});
        quat_t rotation;
        glm_quat_mul(rotateY.q, originalRotation.q, rotation.q);
        glm_quat_mul(rotation.q, rotateX.q, rotation.q);

        vec3_t up;
        glm_quat_rotatev(rotation.q, (vec3){0, 1, 0}, up.v);
        if (up.v[1] < 0) {
            // Camera is turning upside-down, reset it
            vec3_t right;
            glm_quat_rotatev(rotation.q, (vec3){1, 0, 0}, right.v);
            right.v[1] = 0;
            up.v[1] = 0;

            vec3_t forward;
            glm_cross(right.v, up.v, forward.v);

            glm_normalize(right.v);
            glm_normalize(up.v);
            glm_normalize(forward.v);

            glm_vec3_copy(right.v, transform->transform.rotate.m[0]);
            glm_vec3_copy(up.v, transform->transform.rotate.m[1]);
            glm_vec3_copy(forward.v, transform->transform.rotate.m[2]);

            // char buffer[256] = {0};
            // snprintf(buffer, 255, "[camera_view] Looking %s\n", forward.v[1] < 0 ? "up" : "down");
            // sp_log_message(SP_LOG_LEVEL_LOG, buffer);
        } else {
            sp_transform_set_rotation(&transform->transform, &rotation);
        }
    }
}

SP_EXPORT size_t sp_library_get_script_definitions(sp_dynamic_script_definition_t *output, size_t output_size) {
    if (output_size >= 1 && output != NULL) {
        sp_string_set(&output[0].name, "camera_view2");

        output[0].type = SP_SCRIPT_TYPE_PHYSICS_SCRIPT;
        output[0].filter_on_event = true;
        string_t *events = sp_string_vector_resize(&output[0].events, 1);
        sp_string_set(&events[0], "/script/camera_rotate");

        output[0].context_size = sizeof(script_camera_view_t);
        output[0].init_func = &camera_view_init;
        output[0].on_tick_func = &camera_view_on_tick;
    }
    return 1;
}
