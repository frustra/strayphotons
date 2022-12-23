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

    struct InternalScriptBase {
        const StructMetadata &metadata;
        InternalScriptBase(const StructMetadata &metadata) : metadata(metadata) {}
        virtual void *Access(ScriptState &state) const = 0;
        // May return nullptr if state has never been accessed.
        virtual const void *Access(const ScriptState &state) const = 0;
    };

    struct ScriptDefinition {
        std::string name;
        std::vector<std::string> events;
        bool filterOnEvent = false;
        const InternalScriptBase *context;
        std::variant<std::monostate, OnTickFunc, OnPhysicsUpdateFunc, PrefabFunc> callback;
    };

    struct ScriptDefinitions {
        robin_hood::unordered_node_map<std::string, ScriptDefinition> scripts;
        robin_hood::unordered_node_map<std::string, ScriptDefinition> prefabs;

        void RegisterScript(ScriptDefinition &&definition);
        void RegisterPrefab(ScriptDefinition &&definition);
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
            if (definition.context) {
                void *dataPtr = definition.context->Access(*this);
                if (!dataPtr) {
                    Errorf("ScriptState::SetParam access returned null data: %s", definition.name);
                    return;
                }
                for (auto &field : definition.context->metadata.fields) {
                    if (field.name == name) {
                        *field.Access<T>(dataPtr) = value;
                        break;
                    }
                }
            } else {
                parameters[name] = value;
            }
        }

        template<typename T>
        T GetParam(std::string name) const {
            if (definition.context) {
                const void *dataPtr = definition.context->Access(*this);
                if (!dataPtr) {
                    Errorf("ScriptState::GetParam access returned null data: %s", definition.name);
                    return {};
                }
                for (auto &field : definition.context->metadata.fields) {
                    if (field.name == name) {
                        return *field.Access<T>(dataPtr);
                    }
                }
                Errorf("ScriptState::GetParam field not found: %s on %s", name, definition.name);
                return {};
            } else {
                auto itr = parameters.find(name);
                if (itr == parameters.end()) {
                    return {};
                } else {
                    return std::get<T>(itr->second);
                }
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

    struct Scripts {
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

    static StructMetadata MetadataScripts(typeid(Scripts),
        StructField::New(&Scripts::scripts, ~FieldAction::AutoApply));
    static Component<Scripts> ComponentScripts("script", MetadataScripts);

    template<>
    void Component<Scripts>::Apply(Scripts &dst, const Scripts &src, bool liveTarget);

    template<typename T>
    struct InternalScript final : public InternalScriptBase {
        void *Access(ScriptState &state) const override {
            if (!state.userData.has_value()) {
                state.userData.emplace<T>();
            }
            return std::any_cast<T>(&state.userData);
        }

        // May return nullptr if state has never been accessed.
        const void *Access(const ScriptState &state) const override {
            if (state.userData.has_value()) {
                return std::any_cast<T>(&state.userData);
            } else {
                return nullptr;
            }
        }

        static void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            T data;
            if (state.userData.has_value()) {
                data = std::any_cast<T>(state.userData);
            }
            data.OnTick(state, lock, ent, interval);
            state.userData = data;
        }

        InternalScript(const std::string &name, const StructMetadata &metadata) : InternalScriptBase(metadata) {
            GetScriptDefinitions().RegisterScript({name, {}, false, this, OnTickFunc(&OnTick)});
        }

        template<typename... Events>
        InternalScript(const std::string &name, const StructMetadata &metadata, bool filterOnEvent, Events... events)
            : InternalScriptBase(metadata) {
            GetScriptDefinitions().RegisterScript({name, {events...}, filterOnEvent, this, OnTickFunc(&OnTick)});
        }
    };

    template<typename T>
    struct InternalPhysicsScript final : public InternalScriptBase {
        void *Access(ScriptState &state) const override {
            if (!state.userData.has_value()) {
                state.userData.emplace<T>();
            }
            return std::any_cast<T>(&state.userData);
        }

        // May return nullptr if state has never been accessed.
        const void *Access(const ScriptState &state) const override {
            if (state.userData.has_value()) {
                return std::any_cast<T>(&state.userData);
            } else {
                return nullptr;
            }
        }

        static void OnPhysicsUpdate(ScriptState &state,
            PhysicsUpdateLock lock,
            Entity ent,
            chrono_clock::duration interval) {
            T data;
            if (state.userData.has_value()) {
                data = std::any_cast<T>(state.userData);
            }
            data.OnPhysicsUpdate(state, lock, ent, interval);
            state.userData = data;
        }

        InternalPhysicsScript(const std::string &name, const StructMetadata &metadata) : InternalScriptBase(metadata) {
            GetScriptDefinitions().RegisterScript({name, {}, false, this, OnPhysicsUpdateFunc(&OnPhysicsUpdate)});
        }

        template<typename... Events>
        InternalPhysicsScript(const std::string &name,
            const StructMetadata &metadata,
            bool filterOnEvent,
            Events... events)
            : InternalScriptBase(metadata) {
            GetScriptDefinitions().RegisterScript(
                {name, {events...}, filterOnEvent, this, OnPhysicsUpdateFunc(&OnPhysicsUpdate)});
        }
    };

    template<typename T>
    struct PrefabScript final : public InternalScriptBase {
        void *Access(ScriptState &state) const override {
            if (!state.userData.has_value()) {
                state.userData.emplace<T>();
            }
            return std::any_cast<T>(&state.userData);
        }

        // May return nullptr if state has never been accessed.
        const void *Access(const ScriptState &state) const override {
            if (state.userData.has_value()) {
                return std::any_cast<T>(&state.userData);
            } else {
                return nullptr;
            }
        }

        static void Prefab(const ScriptState &state, Lock<AddRemove> lock, Entity ent) {
            T data;
            if (state.userData.has_value()) {
                data = std::any_cast<T>(state.userData);
            }
            data.Prefab(state, lock, ent);
        }

        PrefabScript(const std::string &name, const StructMetadata &metadata) : InternalScriptBase(metadata) {
            GetScriptDefinitions().RegisterPrefab({name, {}, false, this, PrefabFunc(&Prefab)});
        }
    };
} // namespace ecs
