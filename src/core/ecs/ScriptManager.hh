/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
#include "common/LockFreeMutex.hh"
#include "common/Logging.hh"
#include "console/CFunc.hh"
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
    struct DynamicScript;

    using PhysicsUpdateLock = Lock<SendEventsLock,
        ReadSignalsLock,
        Read<TransformSnapshot, SceneInfo>,
        Write<TransformTree, OpticalElement, Physics, PhysicsJoints, PhysicsQuery, Signals, LaserLine, VoxelArea>>;

    using ScriptInitFunc = std::function<void(ScriptState &)>;
    using OnTickFunc = std::function<
        void(ScriptState &, const DynamicLock<ReadSignalsLock> &, Entity, chrono_clock::duration)>;
    using OnEventFunc = std::function<void(ScriptState &, const DynamicLock<SendEventsLock> &, Entity, Event)>;
    using PrefabFunc = std::function<void(const ScriptState &, const sp::SceneRef &, Lock<AddRemove>, Entity)>;
    using ScriptCallback = std::variant<std::monostate, OnTickFunc, OnEventFunc, PrefabFunc>;

    enum class ScriptType {
        LogicScript = 0,
        PhysicsScript,
        PrefabScript,
        EventScript,
    };

    struct ScriptDefinitionBase {
        const StructMetadata &metadata;

        ScriptDefinitionBase(const StructMetadata &metadata) : metadata(metadata) {}
        virtual const void *GetDefault() const = 0;
        virtual void *AccessMut(ScriptState &state) const = 0;
        virtual const void *Access(const ScriptState &state) const = 0;

        size_t GetSize() const {
            return metadata.size;
        }
    };

    static StructMetadata MetadataScriptDefinitionBase(typeid(ScriptDefinitionBase),
        sizeof(ScriptDefinitionBase),
        "ScriptDefinitionBase",
        "A generic script context object base class",
        StructFunction::New("AccessMut", "Return a pointer to the script state data", &ScriptDefinitionBase::AccessMut),
        StructFunction::New("Access", "Return a const pointer to the script state data", &ScriptDefinitionBase::Access),
        StructFunction::New("GetSize",
            "Return the size of the script state data in bytes",
            &ScriptDefinitionBase::GetSize));

    struct ScriptDefinition {
        std::string name;
        ScriptType type;
        std::vector<std::string> events;
        bool filterOnEvent = false;
        ScriptDefinitionBase *context = nullptr;
        std::optional<ScriptInitFunc> initFunc;
        ScriptCallback callback;
    };

    static StructMetadata MetadataScriptDefinition(typeid(ScriptDefinition),
        sizeof(ScriptDefinition),
        "ScriptDefinition",
        "A definition describing the name, type, and functions of a script",
        StructField::New("name", "The name of the script", &ScriptDefinition::name),
        StructField::New("type", "The type of the script", &ScriptDefinition::type),
        StructField::New("events", "A list of the names of events this script can receive", &ScriptDefinition::events),
        StructField::New("filter_on_event",
            "True if this script should only run if new events are received",
            &ScriptDefinition::filterOnEvent),
        StructField::New("context",
            "A pointer to the script context object defining this script",
            &ScriptDefinition::context));

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
                void *dataPtr = definition.context->AccessMut(*this);
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

        bool PollEvent(const Lock<Read<EventInput>> &lock, Event &eventOut) const;

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
        sp::LockFreeMutex mutex;
    };

    class ScriptManager {
        sp::LogOnExit logOnExit = "Scripts shut down =====================================================";

    public:
        ScriptManager();
        ~ScriptManager();

        std::shared_ptr<ScriptState> NewScriptInstance(const ScriptState &state, bool runInit);
        std::shared_ptr<ScriptState> NewScriptInstance(const EntityScope &scope, const ScriptDefinition &definition);

        std::shared_ptr<DynamicScript> LoadDynamicScript(const std::string &name);
        void ReloadDynamicScripts();

        void RegisterEvents(const Lock<Read<Name>, Write<EventInput, Scripts>> &lock);
        void RegisterEvents(const Lock<Read<Name>, Write<EventInput, Scripts>> &lock, const Entity &ent);
        void RunOnTick(const Lock<WriteAll> &Lock, const chrono_clock::duration &interval);
        void RunOnPhysicsUpdate(const PhysicsUpdateLock &lock, const chrono_clock::duration &interval);

        // RunPrefabs should only be run from the SceneManager thread
        void RunPrefabs(const Lock<AddRemove> &lock, Entity ent);

    private:
        void internalRegisterEvents(const Lock<Read<Name>, Write<EventInput, Scripts>> &lock,
            const Entity &ent,
            ScriptState &state);

        sp::CFuncCollection funcs;
        sp::EnumArray<ScriptSet, ScriptType> scripts = {};

        sp::LockFreeMutex dynamicScriptMutex;
        robin_hood::unordered_map<std::string, std::shared_ptr<DynamicScript>> dynamicScripts;

        friend class StructMetadata;
        friend class ScriptInstance;
        friend struct sp::EditorContext;
    };

    ScriptManager &GetScriptManager();
    ScriptDefinitions &GetScriptDefinitions();
} // namespace ecs
