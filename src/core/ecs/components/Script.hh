#pragma once

#include "core/Tracing.hh"
#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Events.hh"
#include "ecs/components/Signals.hh"

#include <any>
#include <functional>
#include <robin_hood.h>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ecs {
    class ScriptState;

    using PhysicsUpdateLock = Lock<SendEventsLock,
        ReadSignalsLock,
        Read<TransformSnapshot>,
        Write<TransformTree, Physics, PhysicsJoints, PhysicsQuery, LaserLine, VoxelArea>>;

    using OnTickFunc = std::function<void(ScriptState &, Lock<WriteAll>, Entity, chrono_clock::duration)>;
    using OnPhysicsUpdateFunc = std::function<void(ScriptState &, PhysicsUpdateLock, Entity, chrono_clock::duration)>;
    using PrefabFunc = std::function<void(const ScriptState &, Lock<AddRemove>, Entity)>;

    struct ScriptDefinition {
        std::string name;
        std::vector<std::string> events;
        const StructMetadata *metadata;
        std::function<void *(ScriptState &)> dataAccessor;
        std::variant<std::monostate, OnTickFunc, OnPhysicsUpdateFunc, PrefabFunc> callback;
    };

    struct ScriptDefinitions {
        robin_hood::unordered_node_map<std::string, ScriptDefinition> scripts;
        robin_hood::unordered_node_map<std::string, ScriptDefinition> prefabs;
    };

    ScriptDefinitions &GetScriptDefinitions();

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
        ScriptState(const EntityScope &scope, const ScriptDefinition &definition);
        ScriptState(const EntityScope &scope, OnTickFunc callback);
        ScriptState(const EntityScope &scope, OnPhysicsUpdateFunc callback);
        ScriptState(const EntityScope &scope, PrefabFunc callback);

        template<typename T>
        void SetParam(std::string name, const T &value) {
            parameters[name] = value;
        }

        template<typename T>
        bool HasParam(std::string name) const {
            auto itr = parameters.find(name);
            if (itr == parameters.end()) {
                return false;
            } else {
                return std::holds_alternative<T>(itr->second);
            }
        }

        template<typename T>
        const T &GetParamRef(std::string name) const {
            auto itr = parameters.find(name);
            Assertf(itr != parameters.end(), "script doesn't have parameter %s", name);
            return std::get<T>(itr->second);
        }

        template<typename T>
        T GetParam(std::string name) const {
            auto itr = parameters.find(name);
            if (itr == parameters.end()) {
                return {};
            } else {
                return std::get<T>(itr->second);
            }
        }

        explicit operator bool() const {
            return !std::holds_alternative<std::monostate>(definition.callback);
        }

        bool operator==(const ScriptState &other) const {
            return instanceId == other.instanceId;
        }

        bool operator!=(const ScriptState &other) const {
            return instanceId != other.instanceId;
        }

        size_t GetInstanceId() const {
            return instanceId;
        }

        EntityScope scope;
        ScriptDefinition definition;
        bool filterOnEvent = false;
        ecs::EventQueueRef eventQueue;

        std::any userData;

    private:
        robin_hood::unordered_flat_map<std::string, ParameterType> parameters;
        size_t instanceId;

        friend class StructMetadata;
    };

    static StructMetadata MetadataScriptState(typeid(ScriptState));
    template<>
    bool StructMetadata::Load<ScriptState>(const EntityScope &scope, ScriptState &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<ScriptState>(const EntityScope &scope, picojson::value &dst, const ScriptState &src);

    struct Script {
        ScriptState &AddOnTick(const EntityScope &scope, OnTickFunc callback) {
            return scripts.emplace_back(scope, callback);
        }
        ScriptState &AddOnTick(const EntityScope &scope, const std::string &scriptName) {
            return scripts.emplace_back(scope, GetScriptDefinitions().scripts.at(scriptName));
        }

        ScriptState &AddOnPhysicsUpdate(const EntityScope &scope, OnPhysicsUpdateFunc callback) {
            return scripts.emplace_back(scope, callback);
        }
        ScriptState &AddOnPhysicsUpdate(const EntityScope &scope, const std::string &scriptName) {
            return scripts.emplace_back(scope, GetScriptDefinitions().scripts.at(scriptName));
        }

        ScriptState &AddPrefab(const EntityScope &scope, PrefabFunc callback) {
            return scripts.emplace_back(scope, callback);
        }
        ScriptState &AddPrefab(const EntityScope &scope, const std::string &scriptName) {
            return scripts.emplace_back(scope, GetScriptDefinitions().prefabs.at(scriptName));
        }

        void OnTick(Lock<WriteAll> lock, const Entity &ent, chrono_clock::duration interval);
        void OnPhysicsUpdate(PhysicsUpdateLock lock, const Entity &ent, chrono_clock::duration interval);
        static void Prefab(Lock<AddRemove> lock, const Entity &ent);

        const ScriptState *FindScript(size_t instanceId) const;

        std::vector<ScriptState> scripts;
    };

    static StructMetadata MetadataScript(typeid(Script), StructField::New(&Script::scripts, ~FieldAction::AutoApply));
    static Component<Script> ComponentScript("script", MetadataScript);

    template<>
    void Component<Script>::Apply(Script &dst, const Script &src, bool liveTarget);

    class InternalScript {
    public:
        template<typename... Events>
        InternalScript(const std::string &name, OnTickFunc &&func, Events... events) {
            GetScriptDefinitions().scripts.emplace(name, ScriptDefinition{name, {events...}, nullptr, {}, func});
        }
    };

    class InternalPhysicsScript {
    public:
        template<typename... Events>
        InternalPhysicsScript(const std::string &name, OnPhysicsUpdateFunc &&func, Events... events) {
            GetScriptDefinitions().scripts.emplace(name, ScriptDefinition{name, {events...}, nullptr, {}, func});
        }
    };

    class InternalPrefab {
    public:
        InternalPrefab(const std::string &name, PrefabFunc &&func) {
            GetScriptDefinitions().prefabs.emplace(name, ScriptDefinition{name, {}, nullptr, {}, func});
        }
    };
} // namespace ecs
