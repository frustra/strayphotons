/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "ScriptManager.hh"

#include "common/Defer.hh"
#include "console/CVar.hh"
#include "ecs/DynamicLibrary.hh"
#include "ecs/EcsImpl.hh"

#include <shared_mutex>

namespace ecs {
    ScriptManager &GetScriptManager() {
        static ScriptManager scriptManager;
        return scriptManager;
    }

    ScriptDefinitions &GetScriptDefinitions() {
        static ScriptDefinitions scriptDefinitions;
        return scriptDefinitions;
    }

    static sp::CVar<uint32_t> CVarMaxScriptQueueSize("s.MaxScriptQueueSize",
        EventQueue::MAX_QUEUE_SIZE,
        "Maximum number of event queue size for scripts");

    void ScriptDefinitions::RegisterScript(ScriptDefinition &&definition) {
        Assertf(!scripts.contains(definition.name), "Script definition already exists: %s", definition.name);
        scripts.emplace(definition.name, definition);
    }

    static std::atomic_size_t nextInstanceId;

    ScriptState::ScriptState() : instanceId(++nextInstanceId) {}
    ScriptState::ScriptState(const ScriptState &other)
        : scope(other.scope), definition(other.definition), eventQueue(other.eventQueue), scriptData(other.scriptData),
          instanceId(other.instanceId), index(other.index) {}
    ScriptState::ScriptState(const ScriptDefinition &definition, const EntityScope &scope)
        : scope(scope), definition(definition), instanceId(++nextInstanceId) {}

    Event *ScriptState::PollEvent(const Lock<Read<EventInput>> &lock) {
        if (EventInput::Poll(lock, eventQueue, lastEvent)) return &lastEvent;
        return nullptr;
    }

    ScriptManager::ScriptManager() {
        funcs.Register<std::string>("loadscript",
            "Loads a new dynamic library script by name",
            [this](const std::string &name) {
                LoadDynamicLibrary(name);
            });
        funcs.Register("reloadscripts", "Reloads all dynamically loaded scripts", [this]() {
            ReloadDynamicLibraries();
        });
    }

    ScriptManager::~ScriptManager() {
        // Remove any ScriptStates and EventQueues that are still in use
        {
            auto lock = StartStagingTransaction<Write<Scripts>>();
            for (const Entity &ent : lock.EntitiesWith<Scripts>()) {
                auto &scripts = ent.Get<Scripts>(lock).scripts;
                for (auto &script : scripts) {
                    script.state.reset();
                }
            }
        }
        {
            auto lock = StartTransaction<Write<Scripts, EventInput>>();
            for (const Entity &ent : lock.EntitiesWith<Scripts>()) {
                auto &scripts = ent.Get<Scripts>(lock).scripts;
                for (auto &script : scripts) {
                    script.state.reset();
                }
            }
            for (const Entity &ent : lock.EntitiesWith<EventInput>()) {
                auto &eventInput = ent.Get<EventInput>(lock);
                eventInput.events.clear();
            }
        }
    }

    std::shared_ptr<ScriptState> ScriptManager::NewScriptInstance(const ScriptState &state, bool runInit) {
        DebugZoneScoped;
        auto &scriptSet = scripts[state.definition.type];
        switch (state.definition.type) {
        case ScriptType::LogicScript:
            Assertf(std::holds_alternative<LogicTickFunc>(state.definition.callback),
                "New script %s has mismatched callback type: LogicScript != LogicTickFunc",
                state.definition.name);
            break;
        case ScriptType::PhysicsScript:
            Assertf(std::holds_alternative<PhysicsTickFunc>(state.definition.callback),
                "New script %s has mismatched callback type: PhysicsScript != PhysicsTickFunc",
                state.definition.name);
            break;
        case ScriptType::EventScript:
            Assertf(std::holds_alternative<OnEventFunc>(state.definition.callback),
                "New script %s has mismatched callback type: EventScript != OnEvent",
                state.definition.name);
            break;
        case ScriptType::PrefabScript:
            Assertf(std::holds_alternative<PrefabFunc>(state.definition.callback),
                "New script %s has mismatched callback type: PrefabScript != Prefab",
                state.definition.name);
            break;
        case ScriptType::GuiScript:
            Assertf(std::holds_alternative<GuiRenderableFunc>(state.definition.callback),
                "New script %s has mismatched callback type: GuiScript != GuiRenderable",
                state.definition.name);
            break;
        default:
            Abortf("New script %s has invalid script type: %s", state.definition.name, state.definition.type);
        }

        ScriptState *scriptStatePtr = nullptr;
        {
            DebugZoneScopedN("InitScriptState");
            std::shared_lock l1(dynamicLibraryMutex);
            std::lock_guard l2(scriptSet.mutex);
            size_t newIndex;
            if (scriptSet.freeScriptList.empty()) {
                newIndex = scriptSet.scripts.size();
                scriptSet.scripts.emplace_back(Entity(), state);
            } else {
                newIndex = scriptSet.freeScriptList.top();
                scriptSet.freeScriptList.pop();
                scriptSet.scripts[newIndex] = {Entity(), state};
            }
            auto &newState = scriptSet.scripts[newIndex].second;
            newState.index = newIndex;
            if (runInit) {
                newState.eventQueue = EventQueue::New(CVarMaxScriptQueueSize.Get());
                if (newState.definition.initFunc) (*newState.definition.initFunc)(newState);
                newState.initialized = true;
            }

            scriptStatePtr = &newState;
        }
        return std::shared_ptr<ScriptState>(scriptStatePtr, [this, &scriptSet](ScriptState *state) {
            std::shared_lock l1(dynamicLibraryMutex);
            std::lock_guard l2(scriptSet.mutex);
            if (state->index < scriptSet.scripts.size()) {
                if (state->initialized && state->definition.destroyFunc) (*state->definition.destroyFunc)(*state);
                scriptSet.freeScriptList.push(state->index);
                scriptSet.scripts[state->index] = {};
            }
        });
    }

    std::shared_ptr<ScriptState> ScriptManager::NewScriptInstance(const EntityScope &scope,
        const ScriptDefinition &definition,
        bool runInit) {
        return NewScriptInstance(ScriptState(definition, scope), runInit);
    }

    std::shared_ptr<DynamicLibrary> ScriptManager::LoadDynamicLibrary(const std::string &name) {
        std::lock_guard l(dynamicLibraryMutex);
        auto it = dynamicLibraries.find(name);
        if (it != dynamicLibraries.end()) {
            return it->second;
        }
        auto newLibrary = DynamicLibrary::Load(name);
        if (!newLibrary) {
            Errorf("Failed to load dynamic library: %s", name);
            return nullptr;
        }
        newLibrary->RegisterScripts();
        dynamicLibraries.emplace(name, newLibrary);
        return newLibrary;
    }

    void ScriptManager::ReloadDynamicLibraries() {
        Logf("Reloading DynamicLibraries");
        std::lock_guard l(dynamicLibraryMutex);
        for (size_t i = 0; i < scripts.size(); i++) {
            scripts.at(i).mutex.lock();
        }
        for (auto &[libraryName, dynamicLibrary] : dynamicLibraries) {
            std::vector<ScriptDefinition> previousList;
            // Destroy existing script contexts before reloading
            for (auto &scriptSet : scripts) {
                for (auto &script : scriptSet.scripts) {
                    auto &scriptCtx = script.second.definition.context;
                    if (scriptCtx.expired()) continue;
                    for (auto &dynamicScript : dynamicLibrary->scripts) {
                        auto &reloadingCtx = dynamicScript->definition.context;
                        if (!reloadingCtx.owner_before(scriptCtx) && !scriptCtx.owner_before(reloadingCtx)) {
                            script.second.scriptData.reset();
                            script.second.definition = ScriptDefinition{
                                .name = script.second.definition.name,
                                .type = script.second.definition.type,
                                .context = script.second.definition.context,
                            };
                        }
                    }
                }
            }
            for (auto &dynamicScript : dynamicLibrary->scripts) {
                previousList.emplace_back(dynamicScript->definition);
                GetScriptDefinitions().scripts.erase(dynamicScript->definition.name);
            }
            dynamicLibrary->ReloadLibrary();
            for (auto &dynamicScript : dynamicLibrary->scripts) {
                Logf("%s::%s type: %s",
                    dynamicScript->library->name,
                    dynamicScript->definition.name,
                    dynamicScript->definition.type);
                GetScriptDefinitions().RegisterScript(ScriptDefinition{dynamicScript->definition});

                // Match the new script with the old definition
                auto &newDefinition = dynamicScript->definition;
                const ScriptDefinition *oldDefinition = nullptr;
                for (auto &previous : previousList) {
                    if (previous.name == newDefinition.name && previous.type == newDefinition.type) {
                        oldDefinition = &previous;
                        break;
                    }
                }
                if (oldDefinition) {
                    // Replace instance definitions and reinit
                    for (auto &scriptSet : scripts) {
                        for (auto &script : scriptSet.scripts) {
                            auto &scriptCtx = script.second.definition.context;
                            if (!oldDefinition->context.owner_before(scriptCtx) &&
                                !scriptCtx.owner_before(oldDefinition->context)) {
                                script.second.definition = newDefinition;
                                if (script.second.initialized && newDefinition.initFunc) {
                                    (*newDefinition.initFunc)(script.second);
                                }
                            }
                        }
                    }
                }
            }
        }
        for (size_t i = 0; i < scripts.size(); i++) {
            scripts.at(scripts.size() - i - 1).mutex.unlock();
        }
    }

    std::vector<std::string> ScriptManager::GetDynamicLibraries() const {
        std::vector<std::string> result;
        std::shared_lock l(dynamicLibraryMutex);
        result.reserve(dynamicLibraries.size());
        for (auto &[name, _] : dynamicLibraries) {
            result.emplace_back(name);
        }
        return result;
    }

    void ScriptManager::internalRegisterEvents(const Lock<Read<Name>, Write<EventInput, Scripts>> &lock,
        const Entity &ent,
        ScriptState &state) {
        auto &scriptSet = scripts[state.definition.type];
        Assertf(state.index < scriptSet.scripts.size(), "Invalid script index: %s", state.definition.name);

        auto &entry = scriptSet.scripts[state.index];
        if (!entry.first) {
            if (ent.Has<EventInput>(lock)) {
                auto &eventInput = ent.Get<EventInput>(lock);
                for (auto &event : state.definition.events) {
                    if (!state.eventQueue) state.eventQueue = EventQueue::New();
                    eventInput.Register(lock, state.eventQueue, event);
                }
                entry.first = ent;
            } else if (!state.definition.events.empty()) {
                Warnf("Script %s has events but %s has no EventInput component",
                    state.definition.name,
                    ecs::ToString(lock, ent));
            } else {
                entry.first = ent;
            }
        }
    }

    void ScriptManager::RegisterEvents(const Lock<Read<Name>, Write<EventInput, Scripts>> &lock) {
        ZoneScoped;
        for (size_t i = 0; i < scripts.size(); i++) {
            scripts.at(i).mutex.lock_shared();
        }
        for (const Entity &ent : lock.EntitiesWith<Scripts>()) {
            if (!ent.Has<Scripts>(lock)) continue;
            for (auto &instance : ent.Get<const Scripts>(lock).scripts) {
                if (!instance) continue;
                internalRegisterEvents(lock, ent, *instance.state);
            }
        }
        for (size_t i = 0; i < scripts.size(); i++) {
            scripts.at(scripts.size() - i - 1).mutex.unlock_shared();
        }
    }

    void ScriptManager::RegisterEvents(const Lock<Read<Name>, Write<EventInput, Scripts>> &lock, const Entity &ent) {
        ZoneScoped;
        if (!ent.Has<Scripts>(lock)) return;
        for (auto &instance : ent.Get<const Scripts>(lock).scripts) {
            if (!instance) continue;
            std::shared_lock l1(dynamicLibraryMutex);
            std::shared_lock l2(scripts[instance.state->definition.type].mutex);
            internalRegisterEvents(lock, ent, *instance.state);
        }
    }

    void ScriptManager::RunLogicUpdate(const LogicUpdateLock &lock, const chrono_clock::duration &interval) {
        ZoneScoped;
        auto &scriptSet = scripts[ScriptType::LogicScript];
        std::shared_lock l1(dynamicLibraryMutex);
        std::shared_lock l2(scriptSet.mutex);
        for (auto &[ent, state] : scriptSet.scripts) {
            if (!ent.Has<Scripts>(lock)) continue;
            auto *callback = std::get_if<LogicTickFunc>(&state.definition.callback);
            if (!callback || !*callback) continue;
            if (state.definition.filterOnEvent && state.eventQueue && state.eventQueue->Empty()) continue;
            DebugZoneScopedN("OnTick");
            DebugZoneStr(ecs::ToString(lock, ent));
            (*callback)(state, lock, ent, interval);
            state.lastEvent = {};
        }
    }

    void ScriptManager::RunPhysicsUpdate(const PhysicsUpdateLock &lock, const chrono_clock::duration &interval) {
        ZoneScoped;
        auto &scriptSet = scripts[ScriptType::PhysicsScript];
        std::shared_lock l1(dynamicLibraryMutex);
        std::shared_lock l2(scriptSet.mutex);
        for (auto &[ent, state] : scriptSet.scripts) {
            if (!ent.Has<Scripts>(lock)) continue;
            auto *callback = std::get_if<PhysicsTickFunc>(&state.definition.callback);
            if (!callback || !*callback) continue;
            if (state.definition.filterOnEvent && state.eventQueue && state.eventQueue->Empty()) continue;
            DebugZoneScopedN("OnPhysicsUpdate");
            DebugZoneStr(ecs::ToString(lock, ent));
            (*callback)(state, lock, ent, interval);
            state.lastEvent = {};
        }
    }

    // RunPrefabs should only be run from the SceneManager thread
    void ScriptManager::RunPrefabs(const Lock<AddRemove> &lock, Entity ent) {
        if (!ent.Has<Scripts, SceneInfo>(lock)) return;
        // if (!ent.Get<const Scripts>(lock).HasPrefab()) return;

        ZoneScopedN("RunPrefabs");
        DebugZoneStr(ecs::ToString(lock, ent));

        auto scene = ent.Get<const SceneInfo>(lock).scene;
        Assertf(scene, "RunPrefabs entity has null scene: %s", ecs::ToString(lock, ent));

        // Only lock the mutex if this is a top-level recursive call.
        static thread_local size_t recursionDepth = 0;
        std::optional<sp::Defer<std::function<void()>>> l;
        if (++recursionDepth == 1) {
            scripts[ScriptType::PrefabScript].mutex.lock();
            l.emplace([this]() {
                scripts[ScriptType::PrefabScript].mutex.unlock();
            });
        }

        // Prefab scripts may add additional scripts while iterating.
        // The Scripts component may not remain valid if storage is resized,
        // so we need to reference the lock every loop iteration.
        for (size_t i = 0; i < ent.Get<const Scripts>(lock).scripts.size(); i++) {
            auto instance = ent.Get<const Scripts>(lock).scripts[i];
            if (!instance) continue;
            auto &state = *instance.state;
            auto *callback = std::get_if<PrefabFunc>(&state.definition.callback);
            if (callback && *callback) (*callback)(state, scene, lock, ent);
        }
    }
} // namespace ecs
