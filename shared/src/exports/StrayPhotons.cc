/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "assets/AssetManager.hh"
#include "console/Console.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptManager.hh"
#include "ecs/SignalManager.hh"
#include "game/SceneManager.hh"

#include <strayphotons.h>

SP_EXPORT ConsoleManager *sp_get_console_manager() {
    return &sp::GetConsoleManager();
}

SP_EXPORT ecs::ECSContext *sp_get_ecs_context() {
    return &ecs::GetECSContext();
}

SP_EXPORT ecs::SignalManager *sp_get_signal_manager() {
    return &ecs::GetSignalManager();
}

SP_EXPORT ecs::ScriptManager *sp_get_script_manager() {
    return &ecs::GetScriptManager();
}

SP_EXPORT ecs::ScriptDefinitions *sp_get_script_definitons() {
    return &ecs::GetScriptDefinitions();
}

SP_EXPORT AssetManager *sp_get_asset_manager() {
    return &sp::Assets();
}

SP_EXPORT SceneManager *sp_get_scene_manager() {
    return &sp::GetSceneManager();
}
