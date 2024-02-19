/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "game/CGameContext.hh"
#include "game/SceneManager.hh"

#include <strayphotons.h>
#include <strayphotons/export.h>

using namespace sp;

SP_EXPORT sp_entity_t sp_new_input_device(sp_game_t ctx, const char *name) {
    Assertf(ctx != nullptr, "sp_new_input_device called with null ctx");
    ecs::EntityRef inputEntity = ecs::Name("input", name);
    GetSceneManager()->QueueActionAndBlock(SceneAction::ApplySystemScene,
        "input",
        [&](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
            auto keyboard = scene->NewSystemEntity(lock, scene, inputEntity.Name());
            keyboard.Set<ecs::EventBindings>(lock);
        });
    return (sp_entity_t)inputEntity.GetLive();
}

SP_EXPORT void sp_send_input_bool(sp_game_t ctx, sp_entity_t input_device, const char *event_name, int value) {
    Assertf(ctx != nullptr, "sp_send_input_bool called with null ctx");
    if (ctx->disableInput) return;
    ctx->game.inputEventQueue.PushEvent(ecs::Event{event_name, input_device, (bool)value});
}

SP_EXPORT void sp_send_input_str(sp_game_t ctx, sp_entity_t input_device, const char *event_name, const char *value) {
    Assertf(ctx != nullptr, "sp_send_input_str called with null ctx");
    if (ctx->disableInput) return;
    ctx->game.inputEventQueue.PushEvent(ecs::Event{event_name, input_device, std::string(value)});
}

SP_EXPORT void sp_send_input_int(sp_game_t ctx, sp_entity_t input_device, const char *event_name, int value) {
    Assertf(ctx != nullptr, "sp_send_input_int called with null ctx");
    if (ctx->disableInput) return;
    ctx->game.inputEventQueue.PushEvent(ecs::Event{event_name, input_device, value});
}

SP_EXPORT void sp_send_input_uint(sp_game_t ctx, sp_entity_t input_device, const char *event_name, unsigned int value) {
    Assertf(ctx != nullptr, "sp_send_input_uint called with null ctx");
    if (ctx->disableInput) return;
    ctx->game.inputEventQueue.PushEvent(ecs::Event{event_name, input_device, value});
}

SP_EXPORT void sp_send_input_vec2(sp_game_t ctx,
    sp_entity_t input_device,
    const char *event_name,
    float value_x,
    float value_y) {
    Assertf(ctx != nullptr, "sp_send_input_vec2 called with null ctx");
    if (ctx->disableInput) return;
    ctx->game.inputEventQueue.PushEvent(ecs::Event{event_name, input_device, glm::vec2(value_x, value_y)});
}
