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
// #include <string.h>

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

SP_EXPORT void life_cell_on_tick(void *context,
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
        sp_string_set(&output[0].name, "life_cell");
        output[0].type = SP_SCRIPT_TYPE_LOGIC_SCRIPT;
        output[0].filter_on_event = false;
        string_t *events = sp_string_vector_resize(&output[0].events, 2);
        sp_string_set(&events[0], "/life/neighbor_alive");
        sp_string_set(&events[1], "/life/toggle_alive");
        output[0].context_size = sizeof(script_life_cell_t);
        output[0].on_tick_func = &life_cell_on_tick;

        // sp_string_set(&output[1].name, "prefab_life");
        // output[1].type = SP_SCRIPT_TYPE_PREFAB_SCRIPT;
        // output[1].on_tick_func = &sun_on_tick;
    }
    return 1;
}

// TODO: Implement StructMetadata across DLL boundary
// StructMetadata MetadataLifeCell(typeid(LifeCell),
//     sizeof(LifeCell),
//     "LifeCell",
//     "",
//     StructField::New("alive", &LifeCell::alive),
//     StructField::New("initialized", &LifeCell::initialized, FieldAction::None),
//     StructField::New("neighbor_count", &LifeCell::neighborCount, FieldAction::None));
// LogicScript<LifeCell> lifeCell("life_cell", MetadataLifeCell, false, "/life/neighbor_alive", "/life/toggle_alive");
