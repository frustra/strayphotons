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
#include <math.h>
#include <stdbool.h>
#include <strayphotons/components.h>
#include <strayphotons/logging.h>
#include <string.h>

typedef struct script_flashlight_t {
    sp_entity_ref_t parentEntity;
} script_flashlight_t;

void flashlight_on_tick(void *context,
    sp_script_state_t *state,
    tecs_lock_t *lock,
    tecs_entity_t ent,
    uint64_t intervalNs) {
    script_flashlight_t *ctx = context;
    if (!Tecs_entity_has_bitset(lock, ent, SP_ACCESS_LIGHT | SP_ACCESS_TRANSFORM_TREE)) return;

    sp_ecs_light_t *light = Tecs_entity_get_light(lock, ent);

    sp_entity_ref_t ref = {0};
    sp_entity_ref_new(ent, &ref);

    sp_signal_ref_t onRef = {0};
    sp_signal_ref_new(&ref, "on", &onRef);
    sp_signal_ref_t intensityRef = {0};
    sp_signal_ref_new(&ref, "intensity", &intensityRef);
    sp_signal_ref_t angleRef = {0};
    sp_signal_ref_new(&ref, "angle", &angleRef);
    light->on = sp_signal_ref_get_signal(&onRef, lock, 0) >= 0.5;
    light->intensity = sp_signal_ref_get_signal(&intensityRef, lock, 0);
    light->spot_angle.radians = glm_rad(sp_signal_ref_get_signal(&angleRef, lock, 0));

    sp_event_t *event;
    while ((event = sp_script_state_poll_event(state, lock))) {
        if (strcmp(event->name, "/action/flashlight/toggle") == 0) {
            sp_signal_ref_set_value(&onRef, lock, light->on ? 0.0 : 1.0);
            light->on = !light->on;
        } else if (strcmp(event->name, "/action/flashlight/grab") == 0) {
            sp_ecs_transform_tree_t *tree = Tecs_entity_get_transform_tree(lock, ent);
            if (sp_entity_ref_is_valid(&tree->parent)) {
                sp_ecs_transform_tree_get_global_transform(tree, lock, &tree->transform);
                if (!sp_entity_ref_is_valid(&ctx->parentEntity)) {
                    sp_entity_ref_copy(&tree->parent, &ctx->parentEntity);
                }
                sp_entity_ref_clear(&tree->parent);
            } else {
                if (sp_entity_ref_is_valid(&ctx->parentEntity)) {
                    vec3_t pos = {0, -0.3, 0};
                    sp_transform_set_position(&tree->transform, &pos);
                    quat_t rotation = GLM_QUAT_IDENTITY_INIT;
                    sp_transform_set_rotation(&tree->transform, &rotation);
                    sp_entity_ref_copy(&ctx->parentEntity, &tree->parent);
                } else {
                    sp_ecs_name_t name = {0};
                    sp_entity_ref_name(&ctx->parentEntity, &name);
                    char buffer[256] = {0};
                    snprintf(buffer,
                        255,
                        "Flashlight parent entity is invalid: %s%s%s\n",
                        name.scene,
                        name.scene[0] == '\0' ? "" : ":",
                        name.entity);
                    sp_log_message(SP_LOG_LEVEL_LOG, buffer);
                }
            }
        }
    }
}

void sun_on_tick(void *context, sp_script_state_t *state, tecs_lock_t *lock, tecs_entity_t ent, uint64_t intervalNs) {
    if (!Tecs_entity_has_transform_tree(lock, ent)) return;

    sp_entity_ref_t ref = {0};
    sp_entity_ref_new(ent, &ref);

    sp_signal_ref_t positionRef = {0};
    sp_signal_ref_new(&ref, "position", &positionRef);
    sp_signal_ref_t fixPositionRef = {0};
    sp_signal_ref_new(&ref, "fix_position", &fixPositionRef);
    double sunPos = sp_signal_ref_get_signal(&positionRef, lock, 0);
    if (sp_signal_ref_get_signal(&fixPositionRef, lock, 0) == 0.0) {
        double intervalSeconds = (double)intervalNs / 1e9;
        sunPos += intervalSeconds * (0.05 + fabs(sin(sunPos) * 0.1));
        if (sunPos > M_PI_2) sunPos = -M_PI_2;
        sp_signal_ref_set_value(&positionRef, lock, sunPos);
    }

    sp_ecs_transform_tree_t *tree = Tecs_entity_get_transform_tree(lock, ent);

    quat_t rotation = GLM_QUAT_IDENTITY_INIT;
    sp_transform_set_rotation(&tree->transform, &rotation);
    vec3_t axis = {1, 0, 0};
    sp_transform_rotate_axis(&tree->transform, glm_rad(-90.0), &axis);
    axis = (vec3_t){0, 1, 0};
    sp_transform_rotate_axis(&tree->transform, sunPos, &axis);
    vec3_t pos = {sin(sunPos) * 40.0, cos(sunPos) * 40.0, 0};
    sp_transform_set_position(&tree->transform, &pos);
}

PLUGIN_EXPORT size_t sp_plugin_get_script_definitions(sp_dynamic_script_definition_t *output, size_t output_size) {
    if (output_size >= 2 && output != NULL) {
        strncpy(output[0].name, "flashlight", sizeof(output[0].name) - 1);
        output[0].type = SP_SCRIPT_TYPE_LOGIC_SCRIPT;
        output[0].filter_on_event = false;
        event_name_t *events = sp_event_name_vector_resize(&output[0].events, 2);
        strncpy(events[0], "/action/flashlight/toggle", sizeof(events[0]) - 1);
        strncpy(events[1], "/action/flashlight/grab", sizeof(events[1]) - 1);
        output[0].context_size = sizeof(script_flashlight_t);
        output[0].on_tick_func = &flashlight_on_tick;

        strncpy(output[1].name, "sun", sizeof(output[1].name) - 1);
        output[1].type = SP_SCRIPT_TYPE_LOGIC_SCRIPT;
        output[1].on_tick_func = &sun_on_tick;
    }
    return 2;
}
