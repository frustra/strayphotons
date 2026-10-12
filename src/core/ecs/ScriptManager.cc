/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "ScriptManager.hh"

#include "console/CVar.hh"
#include "ecs/DynamicLibrary.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EventQueue.hh"
#include "ecs/ScriptDefinition.hh"
#include "ecs/ScriptGuiDefinition.hh"
#include "strayphotons/Defer.hh"
#include "strayphotons/DispatchQueue.hh"
#include "strayphotons/Logging.hh"
#include "strayphotons/Utility.hh"

#include <limits>
#include <mutex>
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
        : scope(other.scope), definition(other.definition), eventQueue(other.eventQueue), instanceId(other.instanceId),
          index(other.index) {
        if (other.scriptData) {
            scriptData = other.scriptDataCopier(other.scriptData);
            scriptDataType = other.scriptDataType;
            scriptDataCopier = other.scriptDataCopier;
            scriptDataDeleter = other.scriptDataDeleter;
        }
    }
    ScriptState::ScriptState(ScriptState &&other)
        : scope(other.scope), definition(std::move(other.definition)), eventQueue(std::move(other.eventQueue)),
          instanceId(other.instanceId), index(other.index) {
        if (other.scriptData) {
            scriptData = other.scriptData;
            scriptDataType = other.scriptDataType;
            scriptDataCopier = other.scriptDataCopier;
            scriptDataDeleter = other.scriptDataDeleter;
            other.scriptData = nullptr;
            other.scriptDataType = typeid(void);
            other.scriptDataCopier = nullptr;
            other.scriptDataDeleter = nullptr;
        }
    }
    ScriptState::ScriptState(const ScriptDefinition &definition, const EntityScope &scope)
        : scope(scope), definition(definition), instanceId(++nextInstanceId) {}
    ScriptState::~ScriptState() {
        Reset();
    }

    ScriptState &ScriptState::operator=(const ScriptState &other) {
        Reset();
        scope = other.scope;
        definition = other.definition;
        eventQueue = other.eventQueue;
        instanceId = other.instanceId;
        index = other.index;
        scriptData = other.scriptDataCopier(other.scriptData);
        scriptDataType = other.scriptDataType;
        scriptDataCopier = other.scriptDataCopier;
        scriptDataDeleter = other.scriptDataDeleter;
        return *this;
    }

    ScriptState &ScriptState::operator=(ScriptState &&other) {
        Reset();
        scope = other.scope;
        definition = std::move(other.definition);
        eventQueue = std::move(other.eventQueue);
        instanceId = other.instanceId;
        index = other.index;
        other.scope = {};
        other.instanceId = 0;
        other.index = std::numeric_limits<size_t>::max();
        scriptData = other.scriptData;
        scriptDataType = other.scriptDataType;
        scriptDataCopier = other.scriptDataCopier;
        scriptDataDeleter = other.scriptDataDeleter;
        other.scriptData = nullptr;
        other.scriptDataType = typeid(void);
        other.scriptDataCopier = nullptr;
        other.scriptDataDeleter = nullptr;
        return *this;
    }

    void ScriptState::Reset() {
        if (scriptData) {
            scriptDataDeleter(scriptData);
            scriptData = nullptr;
            scriptDataType = typeid(void);
            scriptDataCopier = nullptr;
            scriptDataDeleter = nullptr;
        }
    }

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
            // TODO: Remove or fix this, there are no script instances stored in staging
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
        ScriptState *scriptStatePtr = nullptr;
        size_t newIndex;
        {
            DebugZoneScopedN("AllocScriptState");
            std::lock_guard l2(scriptSet.mutex);
            if (scriptSet.freeScriptList.empty()) {
                newIndex = scriptSet.scripts.size();
                scriptSet.scripts.emplace_back(Entity(), state);
            } else {
                newIndex = scriptSet.freeScriptList.top();
                scriptSet.freeScriptList.pop();
                if (newIndex >= scriptSet.scripts.size()) scriptSet.scripts.resize(newIndex + 1);
                scriptSet.scripts[newIndex] = {Entity(), state};
            }
            scriptSet.validScriptList.emplace_back(newIndex);
            // Debugf("Active scripts %3.1f%% ++ (%llu / %llu)",
            //     100.0f * scriptSet.validScriptList.size() / scriptSet.scripts.size(),
            //     scriptSet.validScriptList.size(),
            //     scriptSet.scripts.size());
            auto &newState = scriptSet.scripts[newIndex].second;
            newState.index = newIndex;
        }
        WithScriptLock(state.definition, true, [&] {
            DebugZoneScopedN("InitScriptState");
            if (state.definition.logicTickFunc) scriptSet.logicScripts.emplace_back(newIndex);
            if (state.definition.physicsTickFunc) scriptSet.physicsScripts.emplace_back(newIndex);
            if (state.definition.onEventFunc) scriptSet.eventScripts.emplace_back(newIndex);
            if (state.definition.prefabFunc) scriptSet.prefabScripts.emplace_back(newIndex);
            if (state.definition.beforeFrameFunc || state.definition.renderGuiFunc) {
                scriptSet.guiScripts.emplace_back(newIndex);
            }
            auto &newState = scriptSet.scripts[newIndex].second;
            if (runInit) {
                if (state.definition.NeedsEventQueue()) {
                    newState.eventQueue = EventQueue::New(CVarMaxScriptQueueSize.Get());
                }
                if (newState.definition.initFunc) (*newState.definition.initFunc)(newState);
                newState.initialized = true;
            }

            scriptStatePtr = &newState;
        });
        return std::shared_ptr<ScriptState>(scriptStatePtr, [this](ScriptState *state) {
            if (state) {
                RemoveScriptInstance(*state);

                if (state->index == std::numeric_limits<size_t>::max()) return;
                std::lock_guard l(scriptSet.mutex);
                if (state->index >= scriptSet.scripts.size()) return;
                sp::erase(scriptSet.validScriptList, state->index);
                scriptSet.freeScriptList.push(state->index);
                scriptSet.scripts[state->index] = {};
                while (!scriptSet.scripts.empty() && !scriptSet.scripts.back().second) {
                    scriptSet.scripts.pop_back();
                }
                // Debugf("Active scripts %3.1f%% -- (%llu/%llu)",
                //     100.0f * scriptSet.validScriptList.size() / scriptSet.scripts.size(),
                //     scriptSet.validScriptList.size(),
                //     scriptSet.scripts.size());
            }
        });
    }

    std::shared_ptr<ScriptState> ScriptManager::NewScriptInstance(const EntityScope &scope,
        const ScriptDefinition &definition,
        bool runInit) {
        return NewScriptInstance(ScriptState(definition, scope), runInit);
    }

    void ScriptManager::RemoveScriptInstance(ScriptState &state) {
        if (state.index == std::numeric_limits<size_t>::max()) return;
        WithScriptLock(state.definition, true, [&] {
            // TODO: unregister event queue if applicable
            if (state.initialized) {
                if (state.definition.destroyFunc) (*state.definition.destroyFunc)(state);
                state.initialized = false;
            }
            if (state.definition.logicTickFunc) sp::erase(scriptSet.logicScripts, state.index);
            if (state.definition.physicsTickFunc) sp::erase(scriptSet.physicsScripts, state.index);
            if (state.definition.onEventFunc) sp::erase(scriptSet.eventScripts, state.index);
            if (state.definition.prefabFunc) sp::erase(scriptSet.prefabScripts, state.index);
            if (state.definition.beforeFrameFunc || state.definition.renderGuiFunc) {
                sp::erase(scriptSet.guiScripts, state.index);
            }
        });
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
        std::lock_guard l1(dynamicLibraryMutex);
        std::lock_guard l2(scriptSet.mutex);
        std::lock_guard l3(scriptSet.logicScriptMutex);
        std::lock_guard l4(scriptSet.physicsScriptMutex);
        std::lock_guard l5(scriptSet.eventScriptMutex);
        std::lock_guard l6(scriptSet.prefabScriptMutex);
        std::lock_guard l7(scriptSet.guiScriptMutex);
        for (auto &[libraryName, dynamicLibrary] : dynamicLibraries) {
            std::vector<ScriptDefinition> previousList;
            // Destroy existing script contexts before reloading
            for (size_t i : scriptSet.validScriptList) {
                auto &script = scriptSet.scripts[i];
                auto &scriptCtx = script.second.definition.context;
                if (scriptCtx.expired()) continue;
                for (auto &dynamicScript : dynamicLibrary->scripts) {
                    auto &reloadingCtx = dynamicScript->definition.context;
                    if (!reloadingCtx.owner_before(scriptCtx) && !scriptCtx.owner_before(reloadingCtx)) {
                        script.second.Reset();
                        script.second.definition = ScriptDefinition{
                            .name = script.second.definition.name,
                            .context = script.second.definition.context,
                        };
                    }
                }
            }
            for (auto &dynamicScript : dynamicLibrary->scripts) {
                previousList.emplace_back(dynamicScript->definition);
                GetScriptDefinitions().scripts.erase(dynamicScript->definition.name);
            }
            dynamicLibrary->ReloadLibrary();
            for (auto &dynamicScript : dynamicLibrary->scripts) {
                Logf("%s::%s", dynamicScript->library->name, dynamicScript->definition.name);
                GetScriptDefinitions().RegisterScript(ScriptDefinition{dynamicScript->definition});

                // Match the new script with the old definition
                auto &newDefinition = dynamicScript->definition;
                const ScriptDefinition *oldDefinition = nullptr;
                for (auto &previous : previousList) {
                    if (previous.name == newDefinition.name) {
                        oldDefinition = &previous;
                        break;
                    }
                }
                if (oldDefinition) {
                    // Replace instance definitions and reinit
                    for (size_t i : scriptSet.validScriptList) {
                        auto &script = scriptSet.scripts[i];
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

    std::vector<std::string> ScriptManager::GetDynamicLibraries() const {
        std::vector<std::string> result;
        std::shared_lock l(dynamicLibraryMutex);
        result.reserve(dynamicLibraries.size());
        for (auto &[name, _] : dynamicLibraries) {
            result.emplace_back(name);
        }
        return result;
    }

    void ScriptManager::internalRegisterActive(const Lock<Read<Name>, Write<EventInput, GuiElement, Scripts>> &lock,
        const Entity &ent,
        std::shared_ptr<ScriptState> instance) {
        Assertf(instance, "ScriptManager::internalRegisterActive called with null instance");
        ScriptState &state = *instance;
        Assertf(state.index < scriptSet.scripts.size(), "Invalid script index: %s", state.definition.name);

        auto &entry = scriptSet.scripts[state.index];
        if (!entry.first) {
            if (state.definition.NeedsEventQueue()) {
                if (ent.Has<EventInput>(lock)) {
                    auto &eventInput = ent.Get<EventInput>(lock);
                    for (auto &event : state.definition.events) {
                        if (!state.eventQueue) state.eventQueue = EventQueue::New();
                        eventInput.Register(lock, state.eventQueue, event);
                    }
                } else if (!state.definition.events.empty()) {
                    Warnf("Script %s has events but %s has no EventInput component",
                        state.definition.name,
                        ecs::ToString(lock, ent));
                    return;
                }
                if ((state.definition.beforeFrameFunc || state.definition.renderGuiFunc) && ent.Has<GuiElement>(lock)) {
                    auto &guiElement = ent.Get<GuiElement>(lock);
                    if (!guiElement.definition) {
                        guiElement.definition = std::make_shared<ScriptGuiDefinition>(instance, ent);
                    }
                }
            }
            entry.first = ent;
        }
    }

    void ScriptManager::RegisterActive(const Lock<Read<Name>, Write<EventInput, GuiElement, Scripts>> &lock) {
        ZoneScoped;
        std::shared_lock l1(scriptSet.logicScriptMutex);
        std::shared_lock l2(scriptSet.physicsScriptMutex);
        std::shared_lock l3(scriptSet.eventScriptMutex);
        std::shared_lock l4(scriptSet.prefabScriptMutex);
        std::shared_lock l5(scriptSet.guiScriptMutex);
        for (const Entity &ent : lock.EntitiesWith<Scripts>()) {
            if (!ent.Has<Scripts>(lock)) continue;
            for (auto &instance : ent.Get<const Scripts>(lock).scripts) {
                if (!instance) continue;
                internalRegisterActive(lock, ent, instance.state);
            }
        }
    }

    void ScriptManager::RegisterActive(const Lock<Read<Name>, Write<EventInput, GuiElement, Scripts>> &lock,
        const Entity &ent) {
        ZoneScoped;
        if (!ent.Has<Scripts>(lock)) return;
        for (auto &instance : ent.Get<const Scripts>(lock).scripts) {
            if (!instance) continue;
            WithScriptLock(instance.state->definition, false, [&] {
                internalRegisterActive(lock, ent, instance.state);
            });
        }
    }

    void ScriptManager::RunLogicUpdate(const LogicUpdateLock &lock, const chrono_clock::duration &interval) {
        ZoneScoped;
        std::shared_lock l1(dynamicLibraryMutex);
        std::shared_lock l2(scriptSet.logicScriptMutex);
        for (size_t i : scriptSet.logicScripts) {
            auto &[ent, state] = scriptSet.scripts[i];
            if (!ent.Has<Scripts>(lock)) continue;
            if (!state.definition.logicTickFunc) continue;
            if (state.definition.filterOnEvent && state.eventQueue && state.eventQueue->Empty()) continue;
            DebugZoneScopedN("OnLogicTick");
            DebugZoneStr(ecs::ToString(lock, ent));
            state.definition.logicTickFunc.callback(state, lock, ent, interval);
            state.lastEvent = {};
        }
    }

    void ScriptManager::RunPhysicsUpdate(const PhysicsUpdateLock &lock, const chrono_clock::duration &interval) {
        ZoneScoped;
        std::shared_lock l1(dynamicLibraryMutex);
        std::shared_lock l2(scriptSet.physicsScriptMutex);
        for (size_t i : scriptSet.physicsScripts) {
            auto &[ent, state] = scriptSet.scripts[i];
            if (!ent.Has<Scripts>(lock)) continue;
            if (!state.definition.physicsTickFunc) continue;
            if (state.definition.filterOnEvent && state.eventQueue && state.eventQueue->Empty()) continue;
            DebugZoneScopedN("OnPhysicsTick");
            DebugZoneStr(ecs::ToString(lock, ent));
            state.definition.physicsTickFunc.callback(state, lock, ent, interval);
            state.lastEvent = {};
        }
    }

    size_t ScriptManager::RunEventHandlers(const DynamicLock<SendEventsLock> &lock, Entity ent, const Event &event) {
        ZoneScoped;
        DebugZoneStr(ecs::ToString(lock, ent));

        std::shared_lock l1(dynamicLibraryMutex);
        std::shared_lock l2(scriptSet.eventScriptMutex);

        size_t eventCount = 0;
        auto &scripts = ent.Get<const Scripts>(lock);
        for (auto &instance : scripts.scripts) {
            if (!instance) continue;
            auto &state = *instance.state;
            auto &eventFunc = state.definition.onEventFunc;
            if (!eventFunc) continue;
            if (!sp::contains(state.definition.events, event.name)) continue;
            if ((eventFunc.readPermissions & lock.GetDynamicReadPermissions()) == eventFunc.readPermissions &&
                (eventFunc.writePermissions & lock.GetDynamicWritePermissions()) == eventFunc.writePermissions) {
                eventFunc.callback(state, lock, ent, event);
                state.lastEvent = {};
            } else {
                // Not all lock permissions available, queue event async
                // TODO: Sending multiple events can bottleneck here, need a better system to group async events into
                // batched transactions
                QueueTransaction(NewDispatchSource,
                    eventFunc.readPermissions | SendEventsLock::GetReadPermissions(),
                    eventFunc.writePermissions,
                    [this, ent, event, instanceId = instance.GetInstanceId()](const DynamicLock<> &dynamicLock) {
                        ZoneScopedN("RunEventHandlersAsync");
                        auto tryLock = dynamicLock.TryLock<SendEventsLock>();
                        Assertf(tryLock.has_value(), "QueueTransaction returned dynamic lock with missing permissions");
                        DebugZoneStr(ecs::ToString(*tryLock, ent));

                        std::shared_lock l1(dynamicLibraryMutex);
                        std::shared_lock l2(scriptSet.eventScriptMutex);

                        auto &scripts = ent.Get<const Scripts>(*tryLock);
                        for (auto &instance : scripts.scripts) {
                            if (!instance) continue;
                            if (instance.GetInstanceId() == instanceId) {
                                auto &state = *instance.state;
                                auto &eventFunc = state.definition.onEventFunc;
                                Assertf((eventFunc.readPermissions & dynamicLock.GetDynamicReadPermissions()) ==
                                                eventFunc.readPermissions &&
                                            (eventFunc.writePermissions & dynamicLock.GetDynamicWritePermissions()) ==
                                                eventFunc.writePermissions,
                                    "Async event permissions did not match");
                                if (!eventFunc) continue;
                                if (!sp::contains(state.definition.events, event.name)) continue;
                                eventFunc.callback(state, *tryLock, ent, event);
                                state.lastEvent = {};
                                break;
                            }
                        }
                    });
            }
            eventCount++;
        }
        return eventCount;
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
            scriptSet.prefabScriptMutex.lock();
            l.emplace([this]() {
                scriptSet.prefabScriptMutex.unlock();
            });
        }

        // Prefab scripts may add additional scripts while iterating.
        // The Scripts component may not remain valid if storage is resized,
        // so we need to reference the lock every loop iteration.
        for (size_t i = 0; i < ent.Get<const Scripts>(lock).scripts.size(); i++) {
            auto instance = ent.Get<const Scripts>(lock).scripts[i];
            if (!instance) continue;
            auto &state = *instance.state;
            if (!state.definition.prefabFunc) continue;
            state.definition.prefabFunc(state, scene, lock, ent);
        }
    }
} // namespace ecs
