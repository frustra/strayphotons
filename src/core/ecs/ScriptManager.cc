/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "ScriptManager.hh"

#include "common/Defer.hh"
#include "console/CVar.hh"
#include "ecs/EcsImpl.hh"

#include <shared_mutex>

namespace ecs {
    ScriptManager &MakeScriptManager() {
        static ScriptManager scriptManager;
        return scriptManager;
    }

    ScriptManager *GetScriptManager(ScriptManager *override) {
        static ScriptManager *overrideValue = nullptr;
        if (override) overrideValue = override;
        if (overrideValue) return overrideValue;
        // return &MakeScriptManager();
        Abortf("Requested ScriptManager before it was set");
    }

    ScriptDefinitions &MakeScriptDefinitions() {
        static ScriptDefinitions scriptDefinitions;
        return scriptDefinitions;
    }

    ScriptDefinitions *GetScriptDefinitions(ScriptDefinitions *override) {
        static ScriptDefinitions *overrideValue = nullptr;
        if (override) overrideValue = override;
        if (overrideValue) return overrideValue;
        // return &MakeScriptDefinitions();
        Abortf("Requested ScriptDefinitions before it was set");
    }

    static sp::CVar<uint32_t> CVarMaxScriptQueueSize("s.MaxScriptQueueSize",
        EventQueue::MAX_QUEUE_SIZE,
        "Maximum number of event queue size for scripts");

    void ScriptDefinitions::RegisterScript(ScriptDefinition &&definition) {
        Assertf(!scripts.contains(definition.name), "Script definition already exists: %s", definition.name);
        scripts.emplace(definition.name, definition);
    }

    void ScriptDefinitions::RegisterPrefab(ScriptDefinition &&definition) {
        Assertf(!prefabs.contains(definition.name), "Prefab definition already exists: %s", definition.name);
        prefabs.emplace(definition.name, definition);
    }

    static std::atomic_size_t nextInstanceId;

    ScriptState::ScriptState() : instanceId(++nextInstanceId) {}
    ScriptState::ScriptState(const ScriptState &other)
        : scope(other.scope), definition(other.definition), eventQueue(other.eventQueue), userData(other.userData),
          instanceId(other.instanceId), index(other.index) {}
    ScriptState::ScriptState(const ScriptDefinition &definition, const EntityScope &scope)
        : scope(scope), definition(definition), instanceId(++nextInstanceId) {}

    ScriptManager::~ScriptManager() {
        // Remove any ScriptStates and EventQueues that are still in use
        {
            auto lock = StartStagingTransaction<Write<Scripts>>();
            for (auto &ent : lock.EntitiesWith<Scripts>()) {
                auto &scripts = ent.Get<Scripts>(lock).scripts;
                for (auto &script : scripts) {
                    script.state.reset();
                }
            }
        }
        {
            auto lock = StartTransaction<Write<Scripts, EventInput>>();
            for (auto &ent : lock.EntitiesWith<Scripts>()) {
                auto &scripts = ent.Get<Scripts>(lock).scripts;
                for (auto &script : scripts) {
                    script.state.reset();
                }
            }
            for (auto &ent : lock.EntitiesWith<EventInput>()) {
                auto &eventInput = ent.Get<EventInput>(lock);
                eventInput.events.clear();
            }
        }
    }

    std::shared_ptr<ScriptState> ScriptManager::NewScriptInstance(const ScriptState &state) {
        ZoneScoped;
        auto *scriptListPtr = scriptLists[state.definition.callback.index()];
        Assertf(scriptListPtr, "Invalid script callback type: %s", state.definition.name);
        auto &scriptList = *scriptListPtr;
        auto &scriptMutex = mutexes[state.definition.callback.index()];

        auto it = scriptList.end();
        EventQueue *eventQueuePtr = nullptr;
        {
            // ZoneScopedN("InitEventQueue");
            std::lock_guard l(mutexes[0]);
            auto queueIt = std::find_if(eventQueues.begin(), eventQueues.end(), [](auto &arg) {
                return arg.Capacity() == 0;
            });
            if (queueIt == eventQueues.end()) {
                // ZoneScopedN("NewEventQueue");
                eventQueuePtr = &eventQueues.emplace_back(CVarMaxScriptQueueSize.Get());
            } else {
                ZoneScopedN("ResizeEventQueue");
                queueIt->Resize(CVarMaxScriptQueueSize.Get());
                eventQueuePtr = &*queueIt;
            }
        }
        auto eventQueue = std::shared_ptr<EventQueue>(eventQueuePtr, [this](EventQueue *queue) {
            std::lock_guard l(mutexes[0]);
            auto it = std::find_if(eventQueues.begin(), eventQueues.end(), [&queue](auto &arg) {
                return &arg == queue;
            });
            Assertf(it != eventQueues.end(), "EventQueue not found for removal");
            it->Resize(0);
        });
        {
            // ZoneScopedN("InitScriptState");
            std::lock_guard l(scriptMutex);
            it = std::find_if(scriptList.begin(), scriptList.end(), [](auto &arg) {
                return arg.second.index == std::numeric_limits<size_t>::max();
            });
            if (it == scriptList.end()) {
                it = scriptList.insert(scriptList.end(), {Entity(), state});
            } else {
                *it = {Entity(), state};
            }
            auto &newState = it->second;
            newState.index = it - scriptList.begin();
            newState.eventQueue = eventQueue;
            if (newState.definition.initFunc) (*newState.definition.initFunc)(newState);
        }

        return std::shared_ptr<ScriptState>(&it->second, [&scriptList, &scriptMutex](ScriptState *state) {
            std::lock_guard l(scriptMutex);
            scriptList[state->index] = {};
        });
    }

    std::shared_ptr<ScriptState> ScriptManager::NewScriptInstance(const EntityScope &scope,
        const ScriptDefinition &definition) {
        return NewScriptInstance(ScriptState(definition, scope));
    }

    void ScriptManager::internalRegisterEvents(const Lock<Read<Name, Scripts>, Write<EventInput>> &lock,
        const Entity &ent,
        const ScriptState &state) const {
        auto *scriptListPtr = scriptLists[state.definition.callback.index()];
        if (!scriptListPtr) return;
        auto &scriptList = *scriptListPtr;
        Assertf(state.index < scriptList.size(), "Invalid script index: %s", state.definition.name);

        auto &entry = scriptList[state.index];
        if (!entry.first) {
            if (ent.Has<EventInput>(lock)) {
                auto &eventInput = ent.Get<EventInput>(lock);
                for (auto &event : state.definition.events) {
                    eventInput.Register(lock, state.eventQueue, event);
                }
            } else if (!state.definition.events.empty()) {
                Warnf("Script %s has events but %s has no EventInput component",
                    state.definition.name,
                    ecs::ToString(lock, ent));
            }
            entry.first = ent;
        }
    }

    void ScriptManager::RegisterEvents(const Lock<Read<Name, Scripts>, Write<EventInput>> &lock) {
        ZoneScoped;
        for (size_t i = 1; i < mutexes.size(); i++) {
            mutexes[i].lock_shared();
        }
        for (auto &ent : lock.EntitiesWith<ecs::Scripts>()) {
            if (!ent.Has<Scripts>(lock)) continue;
            for (auto &instance : ent.Get<const Scripts>(lock).scripts) {
                if (!instance) continue;
                internalRegisterEvents(lock, ent, *instance.state);
            }
        }
        for (size_t i = mutexes.size() - 1; i > 0; i--) {
            mutexes[i].unlock_shared();
        }
    }

    void ScriptManager::RegisterEvents(const Lock<Read<Name, Scripts>, Write<EventInput>> &lock, const Entity &ent) {
        ZoneScoped;
        if (!ent.Has<Scripts>(lock)) return;
        for (auto &instance : ent.Get<const Scripts>(lock).scripts) {
            if (!instance) continue;
            std::shared_lock l(mutexes[instance.state->definition.callback.index()]);
            internalRegisterEvents(lock, ent, *instance.state);
        }
    }

    void ScriptManager::RunOnTick(const Lock<WriteAll> &lock, const chrono_clock::duration &interval) {
        ZoneScoped;
        std::shared_lock l(mutexes[ScriptCallbackIndex<OnTickFunc>()]);
        for (auto &[ent, state] : onTickScripts) {
            if (!ent) continue;
            auto &callback = std::get<OnTickFunc>(state.definition.callback);
            if (state.definition.filterOnEvent && state.eventQueue && state.eventQueue->Empty()) continue;
            // ZoneScopedN("OnTick");
            // ZoneStr(ecs::ToString(lock, ent));
            callback(state, lock, ent, interval);
        }
    }

    void ScriptManager::RunOnPhysicsUpdate(const PhysicsUpdateLock &lock, const chrono_clock::duration &interval) {
        ZoneScoped;
        std::shared_lock l(mutexes[ScriptCallbackIndex<OnPhysicsUpdateFunc>()]);
        for (auto &[ent, state] : onPhysicsUpdateScripts) {
            if (!ent) continue;
            auto &callback = std::get<OnPhysicsUpdateFunc>(state.definition.callback);
            if (state.definition.filterOnEvent && state.eventQueue && state.eventQueue->Empty()) continue;
            // ZoneScopedN("OnPhysicsUpdate");
            // ZoneStr(ecs::ToString(lock, ent));
            callback(state, lock, ent, interval);
        }
    }

    // RunPrefabs should only be run from the SceneManager thread
    void ScriptManager::RunPrefabs(const Lock<AddRemove> &lock, Entity ent) {
        if (!ent.Has<Scripts, SceneInfo>(lock)) return;
        // if (!ent.Get<const Scripts>(lock).HasPrefab()) return;

        ZoneScopedN("RunPrefabs");
        // ZoneStr(ecs::ToString(lock, ent));

        auto scene = ent.Get<const SceneInfo>(lock).scene;
        Assertf(scene, "RunPrefabs entity has null scene: %s", ecs::ToString(lock, ent));

        // Only lock the mutex if this is a top-level recursive call.
        static thread_local size_t recursionDepth = 0;
        std::optional<sp::Defer<std::function<void()>>> l;
        if (++recursionDepth == 1) {
            mutexes[ScriptCallbackIndex<PrefabFunc>()].lock();
            l.emplace([this]() {
                mutexes[ScriptCallbackIndex<PrefabFunc>()].unlock();
            });
        }

        // Prefab scripts may add additional scripts while iterating.
        // The Scripts component may not remain valid if storage is resized,
        // so we need to reference the lock every loop iteration.
        for (size_t i = 0; i < ent.Get<const Scripts>(lock).scripts.size(); i++) {
            auto instance = ent.Get<const Scripts>(lock).scripts[i];
            if (!instance) continue;
            auto &state = *instance.state;
            auto callback = std::get_if<PrefabFunc>(&state.definition.callback);
            if (callback && *callback) (*callback)(state, scene, lock, ent);
        }
    }
} // namespace ecs
