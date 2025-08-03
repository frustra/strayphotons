/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <c_abi/Tecs.hh>
#include <c_abi/strayphotons_ecs_c_abi_entity_gen.h>
#include <c_abi/strayphotons_ecs_c_abi_lock_gen.h>
#include <common/Logging.hh>
#include <glm/glm.hpp>
#include <strayphotons/components.h>

#ifdef SP_EXPORT
    #undef SP_EXPORT
#endif
#ifdef _WIN32
    #define SP_EXPORT __declspec(dllexport)
#else
    #define SP_EXPORT __attribute__((__visibility__("default")))
#endif

static uint32_t instanceCount = 0;

struct ScriptHelloWorld {
    char name[16];
    uint32_t frameCount;

    static void DefaultInit(void *context) {
        ScriptHelloWorld *ctx = static_cast<ScriptHelloWorld *>(context);
        snprintf(ctx->name, sizeof(ctx->name) - 1, "hello%u", ++instanceCount);
    }

    static void Init(void *context, sp_script_state_t *state) {
        ScriptHelloWorld *ctx = static_cast<ScriptHelloWorld *>(context);
        Logf("Script %s init %s (old frame: %u)", state->definition.name, ctx->name, ctx->frameCount);
        Logf("Hello: %llx, int32: %llx, state: %llx",
            &typeid(ScriptHelloWorld),
            &typeid(int32_t),
            &typeid(sp_script_state_t));
        ctx->frameCount = 0;
    }

    static void Destroy(void *context, sp_script_state_t *state) {
        ScriptHelloWorld *ctx = static_cast<ScriptHelloWorld *>(context);
        Logf("Script %s destroyed %s at frame %u", state->definition.name, ctx->name, ctx->frameCount);
    }

    static void OnTickLogic(void *context,
        sp_script_state_t *state,
        tecs_lock_t *lock,
        tecs_entity_t ent,
        uint64_t intervalNs) {
        ScriptHelloWorld *ctx = static_cast<ScriptHelloWorld *>(context);
        if (!Tecs_entity_has_renderable(lock, ent)) return;
        sp_ecs_renderable_t *renderable = Tecs_entity_get_renderable(lock, ent);
        renderable->color_override.rgba[0] = sin(ctx->frameCount / 100.0f) * 0.5 + 0.5;
        renderable->color_override.rgba[1] = sin(ctx->frameCount / 100.0f + 1) * 0.5 + 0.5;
        renderable->color_override.rgba[2] = cos(ctx->frameCount / 100.0f) * 0.5 + 0.5;
        renderable->color_override.rgba[3] = 1;
        ctx->frameCount++;
    }

    static void OnTickPhysics(void *context,
        sp_script_state_t *state,
        tecs_lock_t *lock,
        tecs_entity_t ent,
        uint64_t intervalNs) {
        ScriptHelloWorld *ctx = static_cast<ScriptHelloWorld *>(context);
        if (!Tecs_entity_has_bitset(lock, ent, SP_ACCESS_TRANSFORM_TREE | SP_ACCESS_TRANSFORM_SNAPSHOT)) return;
        sp_ecs_transform_tree_t *transformTree = Tecs_entity_get_transform_tree(lock, ent);
        sp_ecs_transform_snapshot_t *transformSnapshot = Tecs_entity_get_transform_snapshot(lock, ent);
        glm::vec3 newPos(std::sin(ctx->frameCount / 100.0f), 1, std::cos(ctx->frameCount / 100.0f));
        sp_transform_set_position(&transformTree->transform, (const vec3_t *)&newPos);
        sp_ecs_transform_tree_get_global_transform(transformTree, lock, &transformSnapshot->transform);
        ctx->frameCount++;
    }
};

extern "C" {

SP_EXPORT size_t sp_plugin_get_script_definitions(sp_dynamic_script_definition_t *output, size_t output_size) {
    if (output_size >= 2 && output != NULL) {
        std::strncpy(output[0].name, "hello_world", sizeof(output[0].name) - 1);
        output[0].type = SP_SCRIPT_TYPE_LOGIC_SCRIPT;
        output[0].filter_on_event = false;
        output[0].context_size = sizeof(ScriptHelloWorld);
        output[0].default_init_func = &ScriptHelloWorld::DefaultInit;
        output[0].init_func = &ScriptHelloWorld::Init;
        output[0].destroy_func = &ScriptHelloWorld::Destroy;
        output[0].on_tick_func = &ScriptHelloWorld::OnTickLogic;

        std::strncpy(output[1].name, "hello_world2", sizeof(output[1].name) - 1);
        output[1].type = SP_SCRIPT_TYPE_PHYSICS_SCRIPT;
        output[1].filter_on_event = false;
        output[1].context_size = sizeof(ScriptHelloWorld);
        output[1].default_init_func = &ScriptHelloWorld::DefaultInit;
        output[1].init_func = &ScriptHelloWorld::Init;
        output[1].destroy_func = &ScriptHelloWorld::Destroy;
        output[1].on_tick_func = &ScriptHelloWorld::OnTickPhysics;

        for (int i = 0; i < 2; i++) {
            sp_struct_field_t *fields = sp_struct_field_vector_resize(&output[i].fields, 1);
            sp_string_set(&fields[0].name, "frame_count");
            fields[0].type.type_index = SP_TYPE_INDEX_INT32;
            fields[0].type.is_trivial = true;
            fields[0].size = sizeof(ScriptHelloWorld::frameCount);
            fields[0].offset = offsetof(ScriptHelloWorld, frameCount);
        }
    }
    return 2;
}

} // extern "C"
