/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <c_abi/Tecs.h>
#include <c_abi/strayphotons_ecs_c_abi_entity_gen.h>
#include <c_abi/strayphotons_ecs_c_abi_lock_gen.h>
// #include <cglm/cglm.h>
// #include <math.h>
#include <stdbool.h>
#include <strayphotons/components.h>
// #include <strayphotons/logging.h>
#include <string.h>

#ifdef SP_EXPORT
    #undef SP_EXPORT
#endif
#ifdef _WIN32
    #define SP_EXPORT __declspec(dllexport)
#else
    #define SP_EXPORT __attribute__((__visibility__("default")))
#endif

typedef struct script_life_cell_t {
    int neighborCount;
    bool alive;
    bool initialized;
} script_life_cell_t;

void life_cell_on_tick(void *context,
    sp_script_state_t *state,
    tecs_lock_t *lock,
    tecs_entity_t ent,
    uint64_t intervalNs) {
    script_life_cell_t *ctx = context;
    if (!ctx->initialized) {
        if (ctx->alive) {
            sp_event_t event = {"/life/notify_neighbors", ent, {SP_EVENT_DATA_TYPE_BOOL, .b = ctx->alive}};
            sp_event_send(lock, ent, &event);
        }
        ctx->initialized = true;
        return;
    }

    bool forceToggle = false;
    sp_event_t *event;
    while ((event = sp_script_state_poll_event(state, lock))) {
        if (strcmp(event->name, "/life/neighbor_alive") == 0) {
            if (event->data.type != SP_EVENT_DATA_TYPE_BOOL) continue;
            bool neighborAlive = event->data.b;
            ctx->neighborCount += neighborAlive ? 1 : -1;
        } else if (strcmp(event->name, "/life/toggle_alive") == 0) {
            forceToggle = true;
        }
    }

    bool nextAlive = ctx->neighborCount == 3 || (ctx->neighborCount == 2 && ctx->alive);
    if (forceToggle || nextAlive != ctx->alive) {
        ctx->alive = !ctx->alive;
        sp_event_t event = {"/life/notify_neighbors", ent, {SP_EVENT_DATA_TYPE_BOOL, .b = ctx->alive}};
        sp_event_send(lock, ent, &event);
    }
}

SP_EXPORT size_t sp_library_get_script_definitions(sp_dynamic_script_definition_t *output, size_t output_size) {
    if (output_size >= 1 && output != NULL) {
        strncpy(output[0].name, "life_cell", sizeof(output[0].name) - 1);
        output[1].desc = "An event handling script to notify neighboring cells when state changes";
        output[0].type = SP_SCRIPT_TYPE_LOGIC_SCRIPT;
        output[0].filter_on_event = false;
        event_name_t *events = sp_event_name_vector_resize(&output[0].events, 2);
        strncpy(events[0], "/life/neighbor_alive", sizeof(events[0]) - 1);
        strncpy(events[1], "/life/toggle_alive", sizeof(events[1]) - 1);
        sp_struct_field_t *fields = sp_struct_field_vector_resize(&output[0].fields, 3);

        sp_string_set(&fields[0].name, "alive");
        fields[0].type.type_index = SP_TYPE_INDEX_BOOL;
        fields[0].type.is_trivial = true;
        fields[0].size = sizeof(bool);
        fields[0].offset = offsetof(script_life_cell_t, alive);

        sp_string_set(&fields[1].name, "initialized");
        fields[1].type.type_index = SP_TYPE_INDEX_BOOL;
        fields[1].type.is_trivial = true;
        fields[1].size = sizeof(bool);
        fields[1].offset = offsetof(script_life_cell_t, initialized);
        fields[1].actions = 0; // None

        sp_string_set(&fields[2].name, "neighbor_count");
        fields[2].type.type_index = SP_TYPE_INDEX_INT32;
        fields[2].type.is_trivial = true;
        fields[2].size = sizeof(int);
        fields[2].offset = offsetof(script_life_cell_t, neighborCount);

        output[0].context_size = sizeof(script_life_cell_t);
        output[0].on_tick_func = &life_cell_on_tick;

        // strncpy(output[1].name, "prefab_life", sizeof(output[1].name) - 1);
        // output[1].type = SP_SCRIPT_TYPE_PREFAB_SCRIPT;
        // output[1].on_tick_func = &sun_on_tick;
    }
    return 1;
}
