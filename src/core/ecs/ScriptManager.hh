/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/LockFreeMutex.hh"
#include "common/Logging.hh"
#include "console/CFunc.hh"
#include "ecs/EventQueue.hh"
#include "ecs/ScriptDefinition.hh"

#include <any>
#include <deque>
#include <limits>
#include <memory>
#include <shared_mutex>
#include <variant>

namespace sp {
    struct EditorContext;
}

namespace ecs {
    class ScriptInstance;
    class DynamicLibrary;

    class ScriptState {
    public:
        using ParameterType = typename std::variant<bool,
            double,
            std::string,
            glm::vec3,
            std::vector<bool>,
            std::vector<double>,
            std::vector<std::string>>;

        ScriptState();
        ScriptState(const ScriptState &other);
        ScriptState(const ScriptDefinition &definition, const EntityScope &scope = {});

        template<typename T>
        void SetParam(std::string name, const T &value) {
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
        T GetParam(std::string name) const {
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
        Event *PollEvent(const Lock<Read<EventInput>> &lock);

        explicit operator bool() const {
            return !std::holds_alternative<std::monostate>(definition.callback);
        }

        // Compare script definition and parameters
        bool operator==(const ScriptState &other) const;

        // Returns true if the two scripts should represent the same instance
        bool CompareOverride(const ScriptState &other) const;

        size_t GetInstanceId() const {
            return instanceId;
        }

        EntityScope scope;
        ScriptDefinition definition;
        ecs::EventQueueRef eventQueue;

        std::any scriptData;

    private:
        size_t instanceId;
        size_t index = std::numeric_limits<size_t>::max();
        bool initialized = false;
        Event lastEvent;

        friend class ScriptInstance;
        friend class ScriptManager;
    };

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
            ArgDesc("lock", ""),
            ArgDesc("event_out", "")));

    struct ScriptSet {
        std::deque<std::pair<Entity, ScriptState>> scripts;
        std::priority_queue<size_t, std::vector<size_t>, std::greater<size_t>> freeScriptList;
        mutable sp::LockFreeMutex mutex;
    };

    class ScriptManager {
        sp::LogOnExit logOnExit = "Scripts shut down =====================================================";

    public:
        ScriptManager();
        ~ScriptManager();

        std::shared_ptr<ScriptState> NewScriptInstance(const ScriptState &state, bool runInit);
        std::shared_ptr<ScriptState> NewScriptInstance(const EntityScope &scope,
            const ScriptDefinition &definition,
            bool runInit = false);

        std::shared_ptr<DynamicLibrary> LoadDynamicLibrary(const std::string &name);
        void ReloadDynamicLibraries();
        std::vector<std::string> GetDynamicLibraries() const;

        void RegisterEvents(const Lock<Read<Name>, Write<EventInput, Scripts>> &lock);
        void RegisterEvents(const Lock<Read<Name>, Write<EventInput, Scripts>> &lock, const Entity &ent);
        void RunLogicUpdate(const LogicUpdateLock &Lock, const chrono_clock::duration &interval);
        void RunPhysicsUpdate(const PhysicsUpdateLock &lock, const chrono_clock::duration &interval);

        // RunPrefabs should only be run from the SceneManager thread
        void RunPrefabs(const Lock<AddRemove> &lock, Entity ent);

        template<typename Fn>
        auto WithGuiScriptLock(Fn &&callback) {
            ZoneScoped;
            auto &scriptSet = scripts[ScriptType::GuiScript];
            std::shared_lock l1(dynamicLibraryMutex);
            std::shared_lock l2(scriptSet.mutex);
            return callback();
        }

    private:
        void internalRegisterEvents(const Lock<Read<Name>, Write<EventInput, Scripts>> &lock,
            const Entity &ent,
            ScriptState &state);

        sp::CFuncCollection funcs;
        sp::EnumArray<ScriptSet, ScriptType> scripts = {};

        mutable sp::LockFreeMutex dynamicLibraryMutex;
        robin_hood::unordered_map<std::string, std::shared_ptr<DynamicLibrary>> dynamicLibraries;

        friend class StructMetadata;
        friend class ScriptInstance;
        friend struct sp::EditorContext;
    };

    ScriptManager &GetScriptManager();
} // namespace ecs
