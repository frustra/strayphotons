/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "strayphotons/InlineString.hh"

#include <Tecs_abi.hh>
#include <glm/glm.hpp>
#include <strayphotons/Logging.hh>
#include <strayphotons/Tecs_abi_gen.h>
#include <strayphotons/components_gen.h>
#include <string>

static uint32_t instanceCount = 0;

TECS_IMPLEMENT_ABI

struct ScriptHelloWorld {
    sp::InlineString<16> name;
    uint32_t logicFrameCount, physicsFrameCount;

    static void DefaultInit(void *context) {
        ScriptHelloWorld *ctx = static_cast<ScriptHelloWorld *>(context);
        ctx->name = "hello" + std::to_string(++instanceCount);
    }

    static void Init(void *context, sp_script_state_t *state) {
        ScriptHelloWorld *ctx = static_cast<ScriptHelloWorld *>(context);
        Logf("Script %s init %s (old frame: %u, %u)",
            sp_string_get_c_str(&state->definition.name),
            ctx->name,
            ctx->logicFrameCount,
            ctx->physicsFrameCount);
        Logf("Hello: %llx, int32: %llx, state: %llx",
            &typeid(ScriptHelloWorld),
            &typeid(int32_t),
            &typeid(sp_script_state_t));
        ctx->logicFrameCount = 0;
        ctx->physicsFrameCount = 0;
    }

    static void Destroy(void *context, sp_script_state_t *state) {
        ScriptHelloWorld *ctx = static_cast<ScriptHelloWorld *>(context);
        Logf("Script %s destroyed %s at frame %u, %u",
            sp_string_get_c_str(&state->definition.name),
            ctx->name,
            ctx->logicFrameCount,
            ctx->physicsFrameCount);
    }

    static void OnLogicTick(void *context,
        sp_script_state_t *state,
        tecs_lock_t *lock,
        tecs_entity_t ent,
        uint64_t intervalNs) {
        ScriptHelloWorld *ctx = static_cast<ScriptHelloWorld *>(context);
        if (!Tecs_entity_has_bitset(lock, ent, SP_ACCESS_RENDERABLE | SP_ACCESS_TRANSFORM_TREE)) return;

        sp_ecs_renderable_t *renderable = Tecs_entity_get_renderable(lock, ent);
        renderable->color_override.rgba[0] = sin(ctx->logicFrameCount / 100.0f) * 0.5 + 0.5;
        renderable->color_override.rgba[1] = sin(ctx->logicFrameCount / 100.0f + 1) * 0.5 + 0.5;
        renderable->color_override.rgba[2] = cos(ctx->logicFrameCount / 100.0f) * 0.5 + 0.5;
        renderable->color_override.rgba[3] = 1;

        sp_ecs_transform_tree_t *transformTree = Tecs_entity_get_transform_tree(lock, ent);
        glm::vec3 newPos(std::sin(ctx->logicFrameCount / 100.0f), 1, std::cos(ctx->logicFrameCount / 100.0f));
        sp_transform_set_position(&transformTree->transform, (const vec3_t *)&newPos);

        ctx->logicFrameCount++;
    }

    static void OnPhysicsTick(void *context,
        sp_script_state_t *state,
        tecs_lock_t *lock,
        tecs_entity_t ent,
        uint64_t intervalNs) {
        ScriptHelloWorld *ctx = static_cast<ScriptHelloWorld *>(context);
        if (!Tecs_entity_has_bitset(lock, ent, SP_ACCESS_TRANSFORM_TREE | SP_ACCESS_TRANSFORM_SNAPSHOT)) return;
        const sp_ecs_transform_tree_t *transformTree = Tecs_entity_const_get_transform_tree(lock, ent);
        sp_ecs_transform_snapshot_t *transformSnapshot = Tecs_entity_get_transform_snapshot(lock, ent);
        sp_ecs_transform_tree_get_global_transform(transformTree, lock, &transformSnapshot->transform);
        ctx->physicsFrameCount++;
    }
};

extern "C" {

PLUGIN_EXPORT size_t sp_plugin_get_script_definitions(sp_dynamic_script_definition_t *output, size_t output_size) {
    if (output_size >= 1 && output != NULL) {
        sp_string_set(&output[0].name, "hello_world");
        output[0].context_size = sizeof(ScriptHelloWorld);
        output[0].default_init_func = &ScriptHelloWorld::DefaultInit;
        output[0].init_func = &ScriptHelloWorld::Init;
        output[0].destroy_func = &ScriptHelloWorld::Destroy;
        output[0].logic_tick_func = &ScriptHelloWorld::OnLogicTick;
        output[0].logic_read_permissions = 1 | SP_ACCESS_RENDERABLE | SP_ACCESS_TRANSFORM_TREE;
        output[0].logic_write_permissions = SP_ACCESS_RENDERABLE | SP_ACCESS_TRANSFORM_TREE;
        output[0].physics_tick_func = &ScriptHelloWorld::OnPhysicsTick;
        output[0].physics_read_permissions = 1 | SP_ACCESS_TRANSFORM_TREE | SP_ACCESS_TRANSFORM_SNAPSHOT;
        output[0].physics_write_permissions = SP_ACCESS_TRANSFORM_SNAPSHOT;

        sp_dynamic_script_definition_add_field(&output[0],
            "logic_frame_count",
            SP_TYPE_INDEX_INT32,
            sizeof(ScriptHelloWorld::logicFrameCount),
            offsetof(ScriptHelloWorld, logicFrameCount));
        sp_dynamic_script_definition_add_field(&output[0],
            "physics_frame_count",
            SP_TYPE_INDEX_INT32,
            sizeof(ScriptHelloWorld::physicsFrameCount),
            offsetof(ScriptHelloWorld, physicsFrameCount));
    }
    return 1;
}

} // extern "C"
