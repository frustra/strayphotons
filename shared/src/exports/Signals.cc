/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "game/CGameContext.hh"

#include <strayphotons.h>
#include <strayphotons/export.h>

SP_EXPORT sp_signal_ref_t sp_get_signal_ref(sp_game_t ctx, sp_entity_ref_t entity_ref, const char *signal_name) {
    Assertf(ctx != nullptr, "sp_get_signal_ref called with null ctx");
    Assertf(entity_ref != nullptr, "sp_get_signal_ref called with null entity_ref");
    return new ecs::SignalRef(*entity_ref, signal_name);
}

// SP_EXPORT float sp_get_signal(sp_game_t ctx, sp_signal_ref_t signal_ref) {
//     Assertf(ctx != nullptr, "sp_get_signal called with null ctx");
//     Assertf(signal_ref != nullptr, "sp_get_signal called with null signal_ref");
//     return signal_ref->GetSignal(lock);
// }

// SP_EXPORT void sp_set_signal(sp_game_t ctx, sp_signal_ref_t signal_ref, float value) {
//     Assertf(ctx != nullptr, "sp_set_signal called with null ctx");
//     Assertf(signal_ref != nullptr, "sp_set_signal called with null signal_ref");
//     if (ctx->disableInput) return;
//     ctx->game.inputEventQueue.PushEvent(ecs::Event{event_name, input_device, std::string(value)});
// }
