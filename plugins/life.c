/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <strayphotons/Tecs_abi_gen.h>
#include <strayphotons/components_gen.h>
#include <string.h>

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

PLUGIN_EXPORT size_t sp_plugin_get_script_definitions(sp_dynamic_script_definition_t *output, size_t output_size) {
    if (output_size >= 1 && output != NULL) {
        sp_string_set(&output[0].name, "life_cell");
        output[0].desc = "An event handling script to notify neighboring cells when state changes";
        output[0].type = SP_SCRIPT_TYPE_LOGIC_SCRIPT;
        output[0].filter_on_event = false;
        sp_dynamic_script_definition_add_event(&output[0], "/life/neighbor_alive");
        sp_dynamic_script_definition_add_event(&output[0], "/life/toggle_alive");
        sp_struct_field_t *fields = sp_struct_field_vector_resize(&output[0].fields, 3);

        sp_dynamic_script_definition_add_field(&output[0],
            "alive",
            SP_TYPE_INDEX_BOOL,
            sizeof(bool),
            offsetof(script_life_cell_t, alive));

        sp_struct_field_t *field = sp_dynamic_script_definition_add_field(&output[0],
            "initialized",
            SP_TYPE_INDEX_BOOL,
            sizeof(bool),
            offsetof(script_life_cell_t, initialized));
        field->actions = 0; // None

        sp_dynamic_script_definition_add_field(&output[0],
            "neighbor_count",
            SP_TYPE_INDEX_INT32,
            sizeof(int),
            offsetof(script_life_cell_t, neighborCount));

        output[0].context_size = sizeof(script_life_cell_t);
        output[0].on_tick_func = &life_cell_on_tick;

        // sp_string_set(&output[1].name, "prefab_life");
        // output[1].type = SP_SCRIPT_TYPE_PREFAB_SCRIPT;
        // output[1].on_tick_func = &sun_on_tick;
    }
    return 1;
}
