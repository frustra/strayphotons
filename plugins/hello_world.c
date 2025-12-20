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
#include <strayphotons/components.h>
#include <strayphotons/logging.h>
#include <string.h>

static uint32_t instanceCount = 0;

typedef struct script_hello_world_t {
    char name[16];
    uint32_t frameCount;
} script_hello_world_t;

void hello_world_default_init(void *context) {
    script_hello_world_t *ctx = context;
    snprintf(ctx->name, sizeof(ctx->name) - 1, "hello%u", ++instanceCount);
}

void hello_world_init(void *context, sp_script_state_t *state) {
    script_hello_world_t *ctx = context;
    char buffer[256] = {0};
    snprintf(buffer, 255, "Script %s init %s (old frame: %u)\n", state->definition.name, ctx->name, ctx->frameCount);
    sp_log_message(SP_LOG_LEVEL_LOG, buffer);
    ctx->frameCount = 0;
}

void hello_world_destroy(void *context, sp_script_state_t *state) {
    script_hello_world_t *ctx = context;
    char buffer[256] = {0};
    snprintf(buffer, 255, "Script %s destroyed %s at frame %u\n", state->definition.name, ctx->name, ctx->frameCount);
    sp_log_message(SP_LOG_LEVEL_LOG, buffer);
}

void hello_world_on_tick_logic(void *context,
    sp_script_state_t *state,
    tecs_lock_t *lock,
    tecs_entity_t ent,
    uint64_t intervalNs) {
    script_hello_world_t *ctx = context;
    if (!Tecs_entity_has_renderable(lock, ent)) return;
    sp_ecs_renderable_t *renderable = Tecs_entity_get_renderable(lock, ent);
    renderable->color_override.rgba[0] = sin(ctx->frameCount / 100.0f) * 0.5 + 0.5;
    renderable->color_override.rgba[1] = sin(ctx->frameCount / 100.0f + 1) * 0.5 + 0.5;
    renderable->color_override.rgba[2] = cos(ctx->frameCount / 100.0f) * 0.5 + 0.5;
    renderable->color_override.rgba[3] = 1;
    ctx->frameCount++;
}

void hello_world_on_tick_physics(void *context,
    sp_script_state_t *state,
    tecs_lock_t *lock,
    tecs_entity_t ent,
    uint64_t intervalNs) {
    script_hello_world_t *ctx = context;
    if (!Tecs_entity_has_bitset(lock, ent, SP_ACCESS_TRANSFORM_TREE | SP_ACCESS_TRANSFORM_SNAPSHOT)) return;
    sp_ecs_transform_tree_t *transformTree = Tecs_entity_get_transform_tree(lock, ent);
    sp_ecs_transform_snapshot_t *transformSnapshot = Tecs_entity_get_transform_snapshot(lock, ent);
    vec3_t newPos = (vec3_t){sin(ctx->frameCount / 100.0f), 1, cos(ctx->frameCount / 100.0f)};
    sp_transform_set_position(&transformTree->transform, &newPos);
    sp_ecs_transform_tree_get_global_transform(transformTree, lock, &transformSnapshot->transform);
    ctx->frameCount++;
}

PLUGIN_EXPORT size_t sp_plugin_get_script_definitions(sp_dynamic_script_definition_t *output, size_t output_size) {
    if (output_size >= 2 && output != NULL) {
        strncpy(output[0].name, "hello_world", sizeof(output[0].name) - 1);
        output[0].type = SP_SCRIPT_TYPE_LOGIC_SCRIPT;
        output[0].filter_on_event = false;
        output[0].context_size = sizeof(script_hello_world_t);
        output[0].default_init_func = &hello_world_default_init;
        output[0].init_func = &hello_world_init;
        output[0].destroy_func = &hello_world_destroy;
        output[0].on_tick_func = &hello_world_on_tick_logic;

        strncpy(output[1].name, "hello_world2", sizeof(output[1].name) - 1);
        output[1].type = SP_SCRIPT_TYPE_PHYSICS_SCRIPT;
        output[1].filter_on_event = false;
        output[1].context_size = sizeof(script_hello_world_t);
        output[1].default_init_func = &hello_world_default_init;
        output[1].init_func = &hello_world_init;
        output[1].destroy_func = &hello_world_destroy;
        output[1].on_tick_func = &hello_world_on_tick_physics;
    }
    return 2;
}
