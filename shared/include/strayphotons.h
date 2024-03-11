/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "strayphotons/entity.h"
#include "strayphotons/export.h"
#include "strayphotons/game.h"
#include "strayphotons/input.h"
#include "strayphotons/logging.h"
#include "strayphotons/signals.h"

#ifdef __cplusplus
namespace ecs {
    class ScriptManager;
    class SignalManager;
    struct ECSContext;
    struct ScriptDefinitions;
} // namespace ecs

namespace sp {
    class AssetManager;
    class ConsoleManager;
    class SceneManager;
} // namespace sp

extern "C" {
#else
typedef void AssetManager;
typedef void ConsoleManager;
typedef void ECSContext;
typedef void SceneManager;
typedef void ScriptDefinitions;
typedef void ScriptManager;
typedef void SignalManager;
#endif

// The following functions are declared in src/exports/StrayPhotons.cc

SP_EXPORT AssetManager *sp_get_asset_manager();
SP_EXPORT ConsoleManager *sp_get_console_manager();
SP_EXPORT ECSContext *sp_get_ecs_context();
SP_EXPORT SceneManager *sp_get_scene_manager();
SP_EXPORT ScriptDefinitions *sp_get_script_definitons();
SP_EXPORT ScriptManager *sp_get_script_manager();
SP_EXPORT SignalManager *sp_get_signal_manager();

#ifdef __cplusplus
}
#endif
