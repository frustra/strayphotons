/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Tracing.hh"
#include "console/CFunc.hh"
#include "ecs/EventQueue.hh"
#include "ecs/EventScriptManager.hh"
#include "ecs/ScriptDefinition.hh"
#include "robin_hood.h"
#include "strayphotons/HeapVector.hh"
#include "strayphotons/LockFreeMutex.hh"
#include "strayphotons/Logging.hh"
#include "strayphotons/Utility.hh"

#include <memory>
#include <memory_resource>
#include <shared_mutex>

namespace sp {
    struct EditorContext;
}

namespace ecs {
    class ScriptInstance;
    class DynamicLibrary;

    class ScriptState : public sp::NonMoveable {
    public:
        ScriptState(const ScriptDefinition &definition, const EntityScope &scope = {});
        ScriptState(const ScriptState &other);
        ~ScriptState();

        template<typename T>
        T *Get();

        template<typename T>
        const T *Get() const;

        template<typename T, typename... Args>
        T *Set(Args &&...args);

        // Create a duplicate of this script with a copy of the state, then run the init function
        std::shared_ptr<ScriptState> CloneInstance(bool runInit) const;

        // Remove this script from the active running script list
        void Deactivate();

        // Delete the current state for this script
        void Reset();

        template<typename T>
        void SetParam(std::string_view name, const T &value) {
            auto ctx = definition.context.lock();
            if (ctx) {
                void *dataPtr = ctx->AccessMut(*this);
                Assertf(dataPtr, "ScriptState::SetParam access returned null data: %s", definition.name);
                for (auto &field : ctx->metadata.fields) {
                    if (field.name == name) {
                        field.Access<T>(dataPtr) = value;
                        break;
                    }
                }
            } else {
                Errorf("ScriptState::SetParam called on definition without context: %s", definition.name);
            }
        }

        template<typename T>
        T GetParam(std::string_view name) const {
            auto ctx = definition.context.lock();
            if (ctx) {
                const void *dataPtr = ctx->Access(*this);
                Assertf(dataPtr, "ScriptState::GetParam access returned null data: %s", definition.name);
                for (auto &field : ctx->metadata.fields) {
                    if (field.name == name) {
                        return field.Access<T>(dataPtr);
                    }
                }
                Errorf("ScriptState::GetParam field not found: %s on %s", name, definition.name);
                return {};
            } else {
                Errorf("ScriptState::GetParam called on definition without context: %s", definition.name);
                return {};
            }
        }

        // The returned pointer remains valid until the next event is polled, or until the end of the script's onTick.
        Event *PollEvent();
        bool HasEvents();

        explicit operator bool() const {
            return (bool)definition;
        }

        // Compare script definition and parameters
        bool operator==(const ScriptState &other) const;

        // Returns true if the two scripts should represent the same instance
        bool CompareOverride(const ScriptState &other) const;

        size_t GetInstanceId() const {
            return instanceId;
        }

        bool IsActive() const {
            return !handle.expired() && ent;
        }

        EntityScope scope;
        ScriptDefinition definition;
        EventQueue eventQueue;

    private:
        std::weak_ptr<ScriptState> handle;
        size_t instanceId;
        Entity ent;
        bool initialized : 1 = false;
        bool logicTickActive : 1 = false;
        bool physicsTickActive : 1 = false;

        EventQueueRef eventQueueHandle;

        void *scriptData = nullptr;
        std::type_index scriptDataType = typeid(void);
        void *(*scriptDataCopier)(const void *scriptData) = nullptr;
        void (*scriptDataDeleter)(void *scriptData) = nullptr;

        friend class ScriptInstance;
        friend class ScriptManager;
    };

    inline bool operator==(const std::weak_ptr<ScriptState> &a, const std::weak_ptr<ScriptState> &b) {
        return !a.owner_before(b) && !b.owner_before(a);
    }

    static StructMetadata MetadataScriptState(typeid(ScriptState),
        sizeof(ScriptState),
        "ScriptState",
        "Stores the definition and state of a script instance",
        StructField::New("scope", "The name scope this script will look for entities in", &ScriptState::scope),
        StructField::New("definition",
            "The definition of the script this struct is holding the state for",
            &ScriptState::definition),
        StructFunction::New("PollEvent",
            "Read the next available script event if there is one",
            &ScriptState::PollEvent,
            ArgDesc("event_out", "")),
        StructFunction::New("HasEvents",
            "Check if there is at least one script event available to read",
            &ScriptState::HasEvents));

    struct ScriptSet {
        sp::LockFreeMutex mutex;
        std::pmr::unsynchronized_pool_resource scriptPool;
        robin_hood::unordered_map<void *, std::weak_ptr<ScriptState>> allScripts;
        // TODO: Switch to unordered maps but ensure scripts on the same entity run in order
        sp::HeapVector<std::weak_ptr<ScriptState>> logicScripts, physicsScripts;
        sp::LockFreeMutex logicScriptMutex, physicsScriptMutex, eventScriptMutex, prefabScriptMutex, guiScriptMutex;
    };

    class ScriptManager {
        sp::LogOnExit logOnExit = "Scripts shut down =====================================================";

    public:
        ScriptManager();
        ~ScriptManager();

        std::shared_ptr<ScriptState> NewScriptInstance(const EntityScope &scope, const ScriptDefinition &definition);
        void ActivateScriptInstance(const std::shared_ptr<ScriptState> &instance, Entity ent);

        std::shared_ptr<DynamicLibrary> LoadDynamicLibrary(const std::string &name);
        void ReloadDynamicLibraries();
        std::vector<std::string> GetDynamicLibraries() const;

        void RegisterActive(const Lock<Read<Name>, Write<EventInput, GuiElement, Scripts>> &lock, const Entity &ent);
        void RunLogicUpdate(const LogicUpdateLock &Lock, const chrono_clock::duration &interval);
        void RunPhysicsUpdate(const PhysicsUpdateLock &lock, const chrono_clock::duration &interval);

        // RunPrefabs should only be run from the SceneManager thread
        void RunPrefabs(const Lock<AddRemove> &lock, Entity ent);

        template<typename Fn>
        auto WithGuiScriptLock(Fn &&callback) {
            ZoneScoped;
            std::shared_lock l(scriptSet.guiScriptMutex);
            return callback();
        }

        template<typename Fn>
        void WithScriptLock(const ScriptDefinition &definition, bool exclusive, Fn &&callback) {
            ZoneScoped;
            if (exclusive) {
                if (definition.logicTickFunc) scriptSet.logicScriptMutex.lock();
                if (definition.physicsTickFunc) scriptSet.physicsScriptMutex.lock();
                if (definition.onEventFunc) scriptSet.eventScriptMutex.lock();
                if (definition.prefabFunc) scriptSet.eventScriptMutex.lock();
                if (definition.beforeFrameFunc || definition.renderGuiFunc) scriptSet.guiScriptMutex.lock();
            } else {
                if (definition.logicTickFunc) scriptSet.logicScriptMutex.lock_shared();
                if (definition.physicsTickFunc) scriptSet.physicsScriptMutex.lock_shared();
                if (definition.onEventFunc) scriptSet.eventScriptMutex.lock_shared();
                if (definition.prefabFunc) scriptSet.eventScriptMutex.lock_shared();
                if (definition.beforeFrameFunc || definition.renderGuiFunc) scriptSet.guiScriptMutex.lock_shared();
            }
            callback();
            if (exclusive) {
                if (definition.logicTickFunc) scriptSet.logicScriptMutex.unlock();
                if (definition.physicsTickFunc) scriptSet.physicsScriptMutex.unlock();
                if (definition.onEventFunc) scriptSet.eventScriptMutex.unlock();
                if (definition.prefabFunc) scriptSet.eventScriptMutex.unlock();
                if (definition.beforeFrameFunc || definition.renderGuiFunc) scriptSet.guiScriptMutex.unlock();
            } else {
                if (definition.logicTickFunc) scriptSet.logicScriptMutex.unlock_shared();
                if (definition.physicsTickFunc) scriptSet.physicsScriptMutex.unlock_shared();
                if (definition.onEventFunc) scriptSet.eventScriptMutex.unlock_shared();
                if (definition.prefabFunc) scriptSet.eventScriptMutex.unlock_shared();
                if (definition.beforeFrameFunc || definition.renderGuiFunc) scriptSet.guiScriptMutex.unlock_shared();
            }
        }

    private:
        sp::CFuncCollection funcs;
        ScriptSet scriptSet = {};

        mutable sp::LockFreeMutex dynamicLibraryMutex;
        robin_hood::unordered_map<std::string, std::shared_ptr<DynamicLibrary>> dynamicLibraries;

        EventScriptManager eventScripts;

        friend class ScriptState;
        friend class StructMetadata;
        friend class ScriptInstance;
        friend struct sp::EditorContext;
        friend class EventScriptManager;
        friend EventScriptManager &GetEventScriptManager();
    };

    ScriptManager &GetScriptManager();
    EventScriptManager &GetEventScriptManager();

    template<typename T>
    T *ScriptState::Get() {
        if (!scriptData) return nullptr;
        Assertf(typeid(T) == scriptDataType,
            "ScriptState::get() called with wrong type: requested %s != %s",
            typeid(T).name(),
            scriptDataType.name());
        return static_cast<T *>(scriptData);
    }

    template<typename T>
    const T *ScriptState::Get() const {
        if (!scriptData) return nullptr;
        Assertf(typeid(T) == scriptDataType,
            "ScriptState::get() called with wrong type: requested %s != %s",
            typeid(T).name(),
            scriptDataType.name());
        return static_cast<const T *>(scriptData);
    }

    template<typename T, typename... Args>
    T *ScriptState::Set(Args &&...args) {
        if (scriptData && typeid(T) == scriptDataType) {
            T *ptr = static_cast<T *>(scriptData);
            *ptr = T(std::forward<Args>(args)...);
            return ptr;
        } else {
            Reset();
            scriptData = GetScriptManager().scriptSet.scriptPool.allocate(sizeof(T), alignof(T));
            scriptDataType = typeid(T);
            scriptDataCopier = [](const void *data) -> void * {
                void *memory = GetScriptManager().scriptSet.scriptPool.allocate(sizeof(T), alignof(T));
                return new (memory) T(*static_cast<const T *>(data));
            };
            scriptDataDeleter = [](void *data) {
                T *ptr = static_cast<T *>(data);
                ptr->~T();
                GetScriptManager().scriptSet.scriptPool.deallocate(ptr, sizeof(T));
            };
            return new (scriptData) T(std::forward<Args>(args)...);
        }
    }
} // namespace ecs
