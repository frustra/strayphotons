/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "ScriptManager.hh"

#include "common/Tracing.hh"
#include "console/CVar.hh"
#include "ecs/DynamicLibrary.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"
#include "ecs/EventQueue.hh"
#include "ecs/EventScriptManager.hh"
#include "ecs/ScriptDefinition.hh"
#include "ecs/ScriptGuiDefinition.hh"
#include "ecs/components/GuiElement.hh"
#include "strayphotons/Logging.hh"
#include "strayphotons/Utility.hh"

#include <memory>
#include <memory_resource>
#include <mutex>
#include <shared_mutex>
#include <threads.h>
#include <tracy/Tracy.hpp>

namespace ecs {
    ScriptManager &GetScriptManager() {
        static ScriptManager scriptManager;
        return scriptManager;
    }

    EventScriptManager &GetEventScriptManager() {
        return GetScriptManager().eventScripts;
    }

    ScriptDefinitions &GetScriptDefinitions() {
        static ScriptDefinitions scriptDefinitions;
        return scriptDefinitions;
    }

    static sp::CVar<uint32_t> CVarMaxScriptQueueSize("s.MaxScriptQueueSize",
        EventQueue().Capacity(),
        "Maximum number of event queue size for scripts");

    void ScriptDefinitions::RegisterScript(ScriptDefinition &&definition) {
        Assertf(!scripts.contains(definition.name), "Script definition already exists: %s", definition.name);
        Assertf(!definition.onEventFunc.readPermissions.test(0),
            "Event script requested invalid ReadPrevious lock permissions: %s",
            definition.name);
        scripts.emplace(definition.name, definition);
    }

    static std::atomic_size_t nextInstanceId;

    ScriptState::ScriptState(const ScriptDefinition &definition, const EntityScope &scope)
        : scope(scope), definition(definition), eventQueue(CVarMaxScriptQueueSize.Get()), instanceId(++nextInstanceId) {
    }
    ScriptState::ScriptState(const ScriptState &other)
        : scope(other.scope), definition(other.definition), eventQueue(other.eventQueue.Capacity()),
          instanceId(other.instanceId) {
        if (other.scriptData) {
            scriptData = other.scriptDataCopier(other.scriptData);
            scriptDataType = other.scriptDataType;
            scriptDataCopier = other.scriptDataCopier;
            scriptDataDeleter = other.scriptDataDeleter;
        }
    }
    ScriptState::~ScriptState() {
        ZoneScoped;
        DebugZoneStr(definition.name);
        Assertf(handle.use_count() == 0, "ScriptState destroyed while referenced");
        Deactivate();
        ScriptManager &scriptManager = GetScriptManager();
        if (initialized) {
            std::shared_lock l(scriptManager.dynamicLibraryMutex);
            // TODO: unregister event queue if applicable
            if (definition.destroyFunc) (*definition.destroyFunc)(*this);
            initialized = false;
        }
        {
            std::lock_guard l(scriptManager.scriptSet.mutex);
            scriptManager.scriptSet.allScripts.erase(this);
            // Debugf("Active scripts-- (%llu)", scriptManager.scriptSet.allScripts.size());
        }
        Reset();
    }

    std::shared_ptr<ScriptState> ScriptState::CloneInstance(bool runInit) const {
        ZoneScoped;
        DebugZoneStr(definition.name);
        ScriptManager &scriptManager = GetScriptManager();
        std::shared_ptr<ScriptState> instance;
        {
            ZoneScopedN("AllocScriptState");
            std::lock_guard l(scriptManager.scriptSet.mutex);
            std::pmr::polymorphic_allocator<ScriptState> allocator(&scriptManager.scriptSet.scriptPool);
            instance = std::allocate_shared<ScriptState>(allocator, *this);
            instance->handle = instance;
            scriptManager.scriptSet.allScripts.emplace(instance.get(), instance);
            // Debugf("Active scripts++ (%llu)", scriptManager.scriptSet.allScripts.size());
        }
        if (runInit) {
            ZoneScopedN("InitScriptState");
            std::shared_lock l(scriptManager.dynamicLibraryMutex);
            if (instance->definition.initFunc) (*instance->definition.initFunc)(*instance);
            instance->initialized = true;
        }
        return instance;
    }

    void ScriptState::Deactivate() {
        ZoneScoped;
        DebugZoneStr(definition.name);
        ScriptManager &scriptManager = GetScriptManager();
        if (!ent) return;
        scriptManager.WithScriptLock(definition, true, [&] {
            if (logicTickActive) sp::erase(scriptManager.scriptSet.logicScripts, handle);
            if (physicsTickActive) sp::erase(scriptManager.scriptSet.physicsScripts, handle);
            eventQueueHandle.reset();
            ent = {};
        });
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

    thread_local Event lastEvent;

    Event *ScriptState::PollEvent() {
        if (definition.onEventFunc) {
            Warnf("Script with onEvent callback cannot poll for events: %s", definition.name);
            return nullptr;
        }
        if (eventQueue.Poll(lastEvent)) return &lastEvent;
        return nullptr;
    }

    bool ScriptState::HasEvents() {
        return !eventQueue.Empty();
    }

    ScriptManager::ScriptManager() : eventScripts(*this) {
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
            for (const Entity &ent : lock.EntitiesWith<EventInput>()) {
                auto &eventInput = ent.Get<EventInput>(lock);
                eventInput.events.clear();
            }
            for (const Entity &ent : lock.EntitiesWith<Scripts>()) {
                auto &scripts = ent.Get<Scripts>(lock).scripts;
                for (auto &script : scripts) {
                    script.state.reset();
                }
            }
        }
    }

    std::shared_ptr<ScriptState> ScriptManager::NewScriptInstance(const EntityScope &scope,
        const ScriptDefinition &definition) {
        ZoneScoped;
        DebugZoneStr(definition.name);
        std::shared_ptr<ScriptState> instance;
        {
            ZoneScopedN("AllocScriptState");
            std::lock_guard l(scriptSet.mutex);
            std::pmr::polymorphic_allocator<ScriptState> allocator(&scriptSet.scriptPool);
            instance = std::allocate_shared<ScriptState>(allocator, definition, scope);
            instance->handle = instance;
            scriptSet.allScripts.emplace(instance.get(), instance);
            // Debugf("Active scripts++ (%llu)", scriptSet.allScripts.size());
        }
        return instance;
    }

    void ScriptManager::ActivateScriptInstance(const std::shared_ptr<ScriptState> &instance, Entity ent) {
        ZoneScoped;
        DebugAssertf(instance, "ScriptManager::ActivateScriptInstance called on null instance");
        DebugZoneStr(instance->definition.name);
        Assertf(!instance->ent, "ScriptManager::ActivateScriptInstance called on active instance");
        WithScriptLock(instance->definition, true, [&] {
            instance->ent = ent;
            if (instance->definition.logicTickFunc) {
                scriptSet.logicScripts.emplace_back(instance);
                instance->logicTickActive = true;
            }
            if (instance->definition.physicsTickFunc) {
                scriptSet.physicsScripts.emplace_back(instance);
                instance->physicsTickActive = true;
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
            for (auto &[_, weakPtr] : scriptSet.allScripts) {
                auto script = weakPtr.lock();
                if (!script) continue;
                auto &scriptCtx = script->definition.context;
                if (scriptCtx.expired()) continue;
                for (auto &dynamicScript : dynamicLibrary->scripts) {
                    auto &reloadingCtx = dynamicScript->definition.context;
                    if (!reloadingCtx.owner_before(scriptCtx) && !scriptCtx.owner_before(reloadingCtx)) {
                        script->Reset();
                        script->definition = ScriptDefinition{
                            .name = script->definition.name,
                            .events = script->definition.events,
                            .context = script->definition.context,
                        };
                    }
                }
            }
            for (auto &dynamicScript : dynamicLibrary->scripts) {
                previousList.emplace_back(dynamicScript->definition);
                GetScriptDefinitions().scripts.erase(dynamicScript->definition.name);
                auto &reloadingCtx = dynamicScript->definition.context;
                Assertf(reloadingCtx.expired(), "Expected old dynamic script library to be expired on reload");
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
                    for (auto &[_, weakPtr] : scriptSet.allScripts) {
                        auto script = weakPtr.lock();
                        if (!script) continue;
                        auto &scriptCtx = script->definition.context;
                        if (!oldDefinition->context.owner_before(scriptCtx) &&
                            !scriptCtx.owner_before(oldDefinition->context)) {
                            script->definition = newDefinition;
                            if (script->initialized && newDefinition.initFunc) {
                                (*newDefinition.initFunc)(*script);
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

    void ScriptManager::RegisterActive(const Lock<Read<Name>, Write<EventInput, GuiElement, Scripts>> &lock,
        const Entity &ent) {
        ZoneScoped;
        if (!ent.Has<Scripts>(lock)) return;
        for (auto &instance : ent.Get<const Scripts>(lock).scripts) {
            if (!instance) continue;
            ScriptState &state = *instance.state;
            if (!state.IsActive()) {
                if (state.definition.NeedsEventQueue()) {
                    if (ent.Has<EventInput>(lock)) {
                        auto &eventInput = ent.Get<EventInput>(lock);
                        if (!state.definition.events.empty()) {
                            DebugAssertf(!state.eventQueueHandle,
                                "ScriptManager::RegisterActive script already has a queue handle: %s",
                                EntityRef(ent).Name().String());
                            state.eventQueueHandle = std::shared_ptr<EventQueue>(&state.eventQueue, [](auto *) {});
#ifdef SP_DEBUG
                            state.eventQueue.handle = state.eventQueueHandle;
#endif
                            for (auto &event : state.definition.events) {
                                eventInput.Register(lock, state.eventQueueHandle, event);
                            }
                        }
                    } else if (!state.definition.events.empty()) {
                        Warnf("Script %s has events but %s has no EventInput component",
                            state.definition.name,
                            ecs::ToString(lock, ent));
                        return;
                    }
                    if ((state.definition.beforeFrameFunc || state.definition.renderGuiFunc) &&
                        ent.Has<GuiElement>(lock)) {
                        auto &guiElement = ent.Get<GuiElement>(lock);
                        if (!guiElement.definition) {
                            guiElement.definition = std::make_shared<ScriptGuiDefinition>(instance.state, ent);
                        }
                    }
                }
                ActivateScriptInstance(instance.state, ent);
            }
        }
    }

    void ScriptManager::RunLogicUpdate(const LogicUpdateLock &lock, const chrono_clock::duration &interval) {
        ZoneScoped;
        std::lock_guard l(scriptSet.logicScriptMutex);
        for (auto &weakPtr : scriptSet.logicScripts) {
            auto script = weakPtr.lock();
            if (!script) continue;
            ScriptState &state = *script;
            if (!state.initialized) continue;
            if (!state.ent.Has<Scripts>(lock)) continue;
            if (!state.definition.logicTickFunc) continue;
            DebugZoneScopedN("OnLogicTick");
            DebugZoneStr(ecs::ToString(lock, state.ent));
            state.definition.logicTickFunc.callback(state, lock, state.ent, interval);
            lastEvent = {};
        }
    }

    void ScriptManager::RunPhysicsUpdate(const PhysicsUpdateLock &lock, const chrono_clock::duration &interval) {
        ZoneScoped;
        std::lock_guard l(scriptSet.physicsScriptMutex);
        for (auto &weakPtr : scriptSet.physicsScripts) {
            auto script = weakPtr.lock();
            if (!script) continue;
            ScriptState &state = *script;
            if (!state.initialized) continue;
            if (!state.ent.Has<Scripts>(lock)) continue;
            if (!state.definition.physicsTickFunc) continue;
            DebugZoneScopedN("OnPhysicsTick");
            DebugZoneStr(ecs::ToString(lock, state.ent));
            state.definition.physicsTickFunc.callback(state, lock, state.ent, interval);
            lastEvent = {};
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
        if (++recursionDepth == 1) scriptSet.prefabScriptMutex.lock();

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

        if (--recursionDepth == 0) scriptSet.prefabScriptMutex.unlock();
    }
} // namespace ecs
