/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
#include "common/LockFreeMutex.hh"
#include "common/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/SignalRef.hh"
#include "ecs/components/Events.hh"

#include <any>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <variant>

namespace sp {
    struct EditorContext;
}

namespace ecs {
    class ScriptState;
    class ScriptInstance;

    using PhysicsUpdateLock = Lock<SendEventsLock,
        ReadSignalsLock,
        Read<TransformSnapshot>,
        Write<TransformTree, OpticalElement, Physics, PhysicsJoints, PhysicsQuery, Signals, LaserLine, VoxelArea>>;

    using ScriptInitFunc = std::function<void(ScriptState &)>;
    using OnTickFunc = std::function<void(ScriptState &, Lock<WriteAll>, Entity, chrono_clock::duration)>;
    using OnPhysicsUpdateFunc = std::function<void(ScriptState &, PhysicsUpdateLock, Entity, chrono_clock::duration)>;
    using PrefabFunc = std::function<void(const ScriptState &, const sp::SceneRef &, Lock<AddRemove>, Entity)>;
    using ScriptCallback = std::variant<std::monostate, OnTickFunc, OnPhysicsUpdateFunc, PrefabFunc>;

    template<typename T>
    struct ScriptCallbackIndex : std::integral_constant<size_t, 0> {};
    template<>
    struct ScriptCallbackIndex<OnTickFunc> : std::integral_constant<size_t, 1> {};
    template<>
    struct ScriptCallbackIndex<OnPhysicsUpdateFunc> : std::integral_constant<size_t, 2> {};
    template<>
    struct ScriptCallbackIndex<PrefabFunc> : std::integral_constant<size_t, 3> {};

    struct InternalScriptBase {
        const StructMetadata &metadata;
        InternalScriptBase(const StructMetadata &metadata) : metadata(metadata) {}
        virtual const void *GetDefault() const = 0;
        virtual void *Access(ScriptState &state) const = 0;
        virtual const void *Access(const ScriptState &state) const = 0;
    };

    struct ScriptDefinition {
        std::string name;
        std::vector<std::string> events;
        bool filterOnEvent = false;
        const InternalScriptBase *context = nullptr;
        std::optional<ScriptInitFunc> initFunc;
        ScriptCallback callback;
    };

    struct ScriptDefinitions {
        std::map<std::string, ScriptDefinition> scripts;
        std::map<std::string, ScriptDefinition> prefabs;

        void RegisterScript(ScriptDefinition &&definition);
        void RegisterPrefab(ScriptDefinition &&definition);
    };

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
            if (definition.context) {
                void *dataPtr = definition.context->Access(*this);
                Assertf(dataPtr, "ScriptState::SetParam access returned null data: %s", definition.name);
                for (auto &field : definition.context->metadata.fields) {
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
            if (definition.context) {
                const void *dataPtr = definition.context->Access(*this);
                Assertf(dataPtr, "ScriptState::GetParam access returned null data: %s", definition.name);
                for (auto &field : definition.context->metadata.fields) {
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

        std::any userData;

    private:
        size_t instanceId;
        size_t index = std::numeric_limits<size_t>::max();

        friend class ScriptInstance;
        friend class ScriptManager;
    };

    class ScriptManager {
        sp::LogOnExit logOnExit = "Scripts shut down =====================================================";

    public:
        ScriptManager() {}
        ~ScriptManager();

        std::shared_ptr<ScriptState> NewScriptInstance(const ScriptState &state, bool runInit);
        std::shared_ptr<ScriptState> NewScriptInstance(const EntityScope &scope, const ScriptDefinition &definition);

        void RegisterEvents(const Lock<Read<Name>, Write<EventInput, Scripts>> &lock);
        void RegisterEvents(const Lock<Read<Name>, Write<EventInput, Scripts>> &lock, const Entity &ent);
        void RunOnTick(const Lock<WriteAll> &Lock, const chrono_clock::duration &interval);
        void RunOnPhysicsUpdate(const PhysicsUpdateLock &lock, const chrono_clock::duration &interval);

        // RunPrefabs should only be run from the SceneManager thread
        void RunPrefabs(const Lock<AddRemove> &lock, Entity ent);

    private:
        void internalRegisterEvents(const Lock<Read<Name>, Write<EventInput, Scripts>> &lock,
            const Entity &ent,
            ScriptState &state) const;

        std::deque<EventQueue> eventQueues;
        std::deque<std::pair<Entity, ScriptState>> onTickScripts;
        std::deque<std::pair<Entity, ScriptState>> onPhysicsUpdateScripts;
        std::deque<std::pair<Entity, ScriptState>> prefabScripts;

        // Mutex index 0 is for eventQeuues, 1+ are for script lists
        std::array<sp::LockFreeMutex, std::variant_size_v<ScriptCallback>> mutexes;

        const std::array<decltype(onTickScripts) *, std::variant_size_v<ScriptCallback>> scriptLists = {
            nullptr, // std::monostate
            &onTickScripts,
            &onPhysicsUpdateScripts,
            &prefabScripts,
        };

        std::priority_queue<size_t, std::vector<size_t>, std::greater<size_t>> freeEventQueues;
        std::array<std::priority_queue<size_t, std::vector<size_t>, std::greater<size_t>>,
            std::variant_size_v<ScriptCallback>>
            freeScriptLists;

        friend class StructMetadata;
        friend class ScriptInstance;
        friend struct sp::EditorContext;
    };

    ScriptManager &GetScriptManager();
    ScriptDefinitions &GetScriptDefinitions();
} // namespace ecs
