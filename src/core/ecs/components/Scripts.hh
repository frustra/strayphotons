#pragma once

#include "core/Tracing.hh"
#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Events.hh"
#include "ecs/components/Signals.hh"
#include "game/SceneRef.hh"

#include <any>
#include <functional>
#include <map>
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
    using PrefabFunc = std::function<void(const ScriptState &, const sp::SceneRef &, Lock<AddRemove>, Entity)>;

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
        std::variant<std::monostate, OnTickFunc, OnPhysicsUpdateFunc, PrefabFunc> callback;
    };

    struct ScriptDefinitions {
        std::map<std::string, ScriptDefinition> scripts;
        std::map<std::string, ScriptDefinition> prefabs;

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
                Assertf(dataPtr, "ScriptState::SetParam access returned null data: %s", definition.name);
                for (auto &field : definition.context->metadata.fields) {
                    if (field.name == name) {
                        *field.Access<T>(dataPtr) = value;
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
                        return *field.Access<T>(dataPtr);
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

        // RunPrefabs should only be run from the SceneManager thread
        static void RunPrefabs(Lock<AddRemove> lock, Entity ent);

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
        const T defaultValue = {};

        const void *GetDefault() const override {
            return &defaultValue;
        }

        void *Access(ScriptState &state) const override {
            void *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            return ptr;
        }

        const void *Access(const ScriptState &state) const override {
            const void *ptr = std::any_cast<T>(&state.userData);
            return ptr ? ptr : &defaultValue;
        }

        static void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            T *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            ptr->OnTick(state, lock, ent, interval);
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
        const T defaultValue = {};

        const void *GetDefault() const override {
            return &defaultValue;
        }

        void *Access(ScriptState &state) const override {
            void *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            return ptr;
        }

        const void *Access(const ScriptState &state) const override {
            const void *ptr = std::any_cast<T>(&state.userData);
            return ptr ? ptr : &defaultValue;
        }

        static void OnPhysicsUpdate(ScriptState &state,
            PhysicsUpdateLock lock,
            Entity ent,
            chrono_clock::duration interval) {
            T *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            ptr->OnPhysicsUpdate(state, lock, ent, interval);
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
        const T defaultValue = {};

        const void *GetDefault() const override {
            return &defaultValue;
        }

        void *Access(ScriptState &state) const override {
            void *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            return ptr;
        }

        const void *Access(const ScriptState &state) const override {
            const void *ptr = std::any_cast<T>(&state.userData);
            return ptr ? ptr : &defaultValue;
        }

        static void Prefab(const ScriptState &state, const sp::SceneRef &scene, Lock<AddRemove> lock, Entity ent) {
            const T *ptr = std::any_cast<T>(&state.userData);
            T data;
            if (ptr) data = *ptr;
            data.Prefab(state, scene.Lock(), lock, ent);
        }

        PrefabScript(const std::string &name, const StructMetadata &metadata) : InternalScriptBase(metadata) {
            GetScriptDefinitions().RegisterPrefab({name, {}, false, this, PrefabFunc(&Prefab)});
        }
    };
} // namespace ecs
