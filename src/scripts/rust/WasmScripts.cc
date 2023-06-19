/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "WasmScripts.hh"

#include "ecs/Ecs.hh"
#include "ecs/ScriptManager.hh"

#include <any>
#include <lib.rs.h>
#include <rust/cxx.h>
#include <string>
#include <unordered_map>

namespace sp::rs {
    void WasmInit(ecs::ScriptState &state) {
        Context **ptr = std::any_cast<Context *>(&state.userData);
        if (!ptr) {
            auto ctx = new_wasm_context(state.definition.name);
            ptr = &state.userData.emplace<Context *>(ctx.into_raw());
        }
    }

    void WasmOnTick(ecs::ScriptState &state,
        ecs::Lock<ecs::WriteAll> lock,
        ecs::Entity ent,
        chrono_clock::duration interval) {
        Context **ptr = std::any_cast<Context *>(&state.userData);
        if (!ptr) {
            auto ctx = new_wasm_context(state.definition.name);
            ptr = &state.userData.emplace<Context *>(ctx.into_raw());
        }

        wasm_run_on_tick(rust::Box<Context>::from_raw(*ptr));
        // ptr->OnPhysicsUpdate(state, lock, ent, interval);
    }

    void WasmOnPhysicsUpdate(ecs::ScriptState &state,
        ecs::PhysicsUpdateLock lock,
        ecs::Entity ent,
        chrono_clock::duration interval) {
        Context **ptr = std::any_cast<Context *>(&state.userData);
        if (!ptr) {
            auto ctx = new_wasm_context(state.definition.name);
            ptr = &state.userData.emplace<Context *>(ctx.into_raw());
        }

        wasm_run_on_physics_update(rust::Box<Context>::from_raw(*ptr));
        // ptr->OnPhysicsUpdate(state, lock, ent, interval);
    }

    void WasmPrefab(const ecs::ScriptState &state,
        const sp::SceneRef &scene,
        ecs::Lock<ecs::AddRemove> lock,
        ecs::Entity ent) {
        // const Context **ptr = std::any_cast<const Context *>(&state.userData);
        // if (!ptr) {
        //     auto ctx = new_wasm_context(state.definition.name);
        //     ptr = &state.userData.emplace<const Context *>(ctx.into_raw());
        // }

        // wasm_run_prefab(rust::Box<Context>::from_raw(*ptr));
        // const T *ptr = std::any_cast<T>(&state.userData);
        // T data;
        // if (ptr) data = *ptr;
        // data.Prefab(state, scene.Lock(), lock, ent);
    }

    void RegisterPrefabScript(rust::String name) {
        Logf("Registered rust Prefab script: %s", name.c_str());

        ecs::GetScriptDefinitions().RegisterScript(ecs::ScriptDefinition{name.c_str(),
            {},
            false,
            nullptr,
            ecs::ScriptInitFunc(&WasmInit),
            ecs::PrefabFunc(&WasmPrefab)});
    }

    void RegisterOnTickScript(rust::String name) {
        Logf("Registered rust OnTick script: %s", name.c_str());
        ecs::GetScriptDefinitions().RegisterScript(ecs::ScriptDefinition{name.c_str(),
            {},
            false,
            nullptr,
            ecs::ScriptInitFunc(&WasmInit),
            ecs::OnTickFunc(&WasmOnTick)});
    }

    void RegisterOnPhysicsUpdateScript(rust::String name) {
        Logf("Registered rust OnPhysicsUpdate script: %s", name.c_str());
        ecs::GetScriptDefinitions().RegisterScript(ecs::ScriptDefinition{name.c_str(),
            {},
            false,
            nullptr,
            ecs::ScriptInitFunc(&WasmInit),
            ecs::OnPhysicsUpdateFunc(&WasmOnPhysicsUpdate)});
    }
} // namespace sp::rs
