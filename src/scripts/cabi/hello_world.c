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

#ifdef SP_EXPORT
    #undef SP_EXPORT
#endif
#ifdef _WIN32
    #define SP_EXPORT __declspec(dllexport)
#else
    #define SP_EXPORT __attribute__((__visibility__("default")))
#endif

static size_t instanceCount = 0;

typedef struct script_hello_world_t {
    char name[16];
    uint64_t frameCount;
} script_hello_world_t;

SP_EXPORT size_t sp_script_get_definition(sp_script_definition_t *output) {
    sp_string_set(&output->name, "hello_world");

    output->type = SP_SCRIPT_TYPE_LOGIC_SCRIPT;
    output->filter_on_event = false;

    return sizeof(script_hello_world_t);
}

SP_EXPORT script_hello_world_t *sp_script_new_context(const script_hello_world_t *existing) {
    script_hello_world_t *ctx = malloc(sizeof(script_hello_world_t));
    memset(ctx->name, 0, sizeof(ctx->name));
    snprintf(ctx->name, sizeof(ctx->name) - 1, "hello%llu", ++instanceCount);
    ctx->frameCount = existing ? existing->frameCount : 0;
    return ctx;
}

SP_EXPORT void sp_script_free_context(script_hello_world_t *ctx) {
    free(ctx);
}

SP_EXPORT void sp_script_init(script_hello_world_t *ctx, sp_script_state_t *state) {
    char buffer[256] = {0};
    snprintf(buffer,
        255,
        "Script %s init %s (old frame: %llu)\n",
        sp_string_get_c_str(&state->definition.name),
        ctx->name,
        ctx->frameCount);
    sp_log_message(SP_LOG_LEVEL_LOG, buffer);
    ctx->frameCount = 0;
}

SP_EXPORT void sp_script_destroy(script_hello_world_t *ctx, sp_script_state_t *state) {
    char buffer[256] = {0};
    snprintf(buffer,
        255,
        "Script %s destroyed %s at frame %llu\n",
        sp_string_get_c_str(&state->definition.name),
        ctx->name,
        ctx->frameCount);
    sp_log_message(SP_LOG_LEVEL_LOG, buffer);
}

SP_EXPORT void sp_script_on_tick(script_hello_world_t *ctx,
    sp_script_state_t *state,
    tecs_lock_t *lock,
    tecs_entity_t ent,
    uint64_t intervalNs) {
    // if (ent.Has<ecs::TransformTree, ecs::TransformSnapshot, ecs::Renderable>(lock)) {
    if (!Tecs_entity_has_bitset(lock,
            ent,
            SP_ACCESS_TRANSFORM_TREE | SP_ACCESS_TRANSFORM_SNAPSHOT | SP_ACCESS_RENDERABLE))
        return;
    sp_ecs_transform_tree_t *transformTree = Tecs_entity_get_transform_tree(lock, ent);
    sp_ecs_transform_snapshot_t *transformSnapshot = Tecs_entity_get_transform_snapshot(lock, ent);
    vec3_t newPos = (vec3_t){sin(ctx->frameCount / 100.0f), 1, cos(ctx->frameCount / 100.0f)};
    sp_transform_set_position(&transformTree->transform, &newPos);
    sp_ecs_transform_tree_get_global_transform(transformTree, lock, &transformSnapshot->transform);

    sp_ecs_renderable_t *renderable = Tecs_entity_get_renderable(lock, ent);
    renderable->color_override.rgba[0] = sin(ctx->frameCount / 100.0f) * 0.5 + 0.5;
    renderable->color_override.rgba[1] = sin(ctx->frameCount / 100.0f + 1) * 0.5 + 0.5;
    renderable->color_override.rgba[2] = cos(ctx->frameCount / 100.0f) * 0.5 + 0.5;
    renderable->color_override.rgba[3] = 1;
    ctx->frameCount++;
}
