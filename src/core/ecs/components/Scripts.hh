#pragma once

#include "core/LockFreeMutex.hh"
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
        Write<TransformTree, OpticalElement, Physics, PhysicsJoints, PhysicsQuery, SignalOutput, LaserLine, VoxelArea>>;

    using ScriptInitFunc = std::function<void(ScriptState &)>;
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
        std::optional<ScriptInitFunc> initFunc;
        std::variant<std::monostate, OnTickFunc, OnPhysicsUpdateFunc, PrefabFunc> callback;
        bool runParallel = false;
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

        sp::LockFreeMutex mutex;
        EntityScope scope;
        ScriptDefinition definition;
        ecs::EventQueueRef eventQueue;

        std::any userData;

    private:
        size_t instanceId;

        friend class StructMetadata;
        friend class ScriptInstance;
    };
    static StructMetadata MetadataScriptState(typeid(ScriptState));

    class ScriptInstance {
    public:
        ScriptInstance() {}
        ScriptInstance(const EntityScope &scope, const ScriptDefinition &definition)
            : state(std::make_shared<ScriptState>(scope, definition)) {}
        ScriptInstance(const EntityScope &scope, OnTickFunc callback, bool parallel = false)
            : ScriptInstance(scope, ScriptDefinition{"", {}, false, nullptr, {}, callback, parallel}) {}
        ScriptInstance(const EntityScope &scope, OnPhysicsUpdateFunc callback, bool parallel = false)
            : ScriptInstance(scope, ScriptDefinition{"", {}, false, nullptr, {}, callback, parallel}) {}
        ScriptInstance(const EntityScope &scope, PrefabFunc callback)
            : ScriptInstance(scope, ScriptDefinition{"", {}, false, nullptr, {}, callback, false}) {}

        explicit operator bool() const {
            return state && *state;
        }

        // Compare script definition and parameters
        bool operator==(const ScriptInstance &other) const {
            return state && other.state && *state == *other.state;
        }

        // Returns true if the two scripts should represent the same instance
        bool CompareOverride(const ScriptInstance &other) const {
            return state && other.state && state->CompareOverride(*other.state);
        }

        size_t GetInstanceId() const {
            if (!state) return 0;
            return state->instanceId;
        }

        std::shared_ptr<ScriptState> state;
    };
    static StructMetadata MetadataScriptInstance(typeid(ScriptInstance));
    template<>
    bool StructMetadata::Load<ScriptInstance>(const EntityScope &scope,
        ScriptInstance &dst,
        const picojson::value &src);
    template<>
    void StructMetadata::Save<ScriptInstance>(const EntityScope &scope,
        picojson::value &dst,
        const ScriptInstance &src,
        const ScriptInstance &def);

    struct Scripts {
        ScriptState &AddOnTick(const EntityScope &scope, const std::string &scriptName) {
            return *(scripts.emplace_back(scope, GetScriptDefinitions().scripts.at(scriptName)).state);
        }

        ScriptState &AddOnTick(const EntityScope &scope, OnTickFunc callback) {
            return *(scripts.emplace_back(scope, callback, false).state);
        }
        ScriptState &AddOnTickParallel(const EntityScope &scope, OnTickFunc callback) {
            return *(scripts.emplace_back(scope, callback, true).state);
        }
        ScriptState &AddOnPhysicsUpdate(const EntityScope &scope, OnPhysicsUpdateFunc callback) {
            return *(scripts.emplace_back(scope, callback, false).state);
        }
        ScriptState &AddOnPhysicsUpdateParallel(const EntityScope &scope, OnPhysicsUpdateFunc callback) {
            return *(scripts.emplace_back(scope, callback, true).state);
        }

        ScriptState &AddPrefab(const EntityScope &scope, PrefabFunc callback) {
            return *(scripts.emplace_back(scope, callback).state);
        }
        ScriptState &AddPrefab(const EntityScope &scope, const std::string &scriptName) {
            return *(scripts.emplace_back(scope, GetScriptDefinitions().prefabs.at(scriptName)).state);
        }

        static void Init(Lock<Read<Name, Scripts>, Write<EventInput>> lock, const Entity &ent);
        void OnTick(Lock<WriteAll> lock, const Entity &ent, chrono_clock::duration interval);
        void OnTickParallel(Lock<WriteAll> lock, chrono_clock::duration interval);
        void OnPhysicsUpdate(PhysicsUpdateLock lock, const Entity &ent, chrono_clock::duration interval);
        void OnPhysicsUpdateParallel(Lock<PhysicsUpdateLock> lock, chrono_clock::duration interval);

        // RunPrefabs should only be run from the SceneManager thread
        static void RunPrefabs(Lock<AddRemove> lock, Entity ent);

        const ScriptState *FindScript(size_t instanceId) const;

        std::vector<ScriptInstance> scripts;
    };

    static StructMetadata MetadataScripts(typeid(Scripts), StructField::New(&Scripts::scripts, FieldAction::AutoLoad));
    static Component<Scripts> ComponentScripts("script", MetadataScripts);

    template<>
    void StructMetadata::Save<Scripts>(const EntityScope &scope,
        picojson::value &dst,
        const Scripts &src,
        const Scripts &def);
    template<>
    void Component<Scripts>::Apply(Scripts &dst, const Scripts &src, bool liveTarget);

    // Cheecks if the script has an Init(ScriptState &state) function
    template<typename T, typename = void>
    struct script_has_init_func : std::false_type {};
    template<typename T>
    struct script_has_init_func<T, std::void_t<decltype(std::declval<T>().Init(std::declval<ScriptState &>()))>>
        : std::true_type {};

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

        static void Init(ScriptState &state) {
            T *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            if constexpr (script_has_init_func<T>()) ptr->Init(state);
        }

        static void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            T *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            ptr->OnTick(state, lock, ent, interval);
        }

        InternalScript(const std::string &name, const StructMetadata &metadata) : InternalScriptBase(metadata) {
            GetScriptDefinitions().RegisterScript(
                {name, {}, false, this, ScriptInitFunc(&Init), OnTickFunc(&OnTick), true});
        }

        template<typename... Events>
        InternalScript(const std::string &name, const StructMetadata &metadata, bool filterOnEvent, Events... events)
            : InternalScriptBase(metadata) {
            GetScriptDefinitions().RegisterScript(
                {name, {events...}, filterOnEvent, this, ScriptInitFunc(&Init), OnTickFunc(&OnTick), true});
        }
    };

    template<typename T>
    struct InternalRootScript final : public InternalScriptBase {
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

        static void Init(ScriptState &state) {
            T *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            if constexpr (script_has_init_func<T>()) ptr->Init(state);
        }

        static void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            T *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            ptr->OnTick(state, lock, ent, interval);
        }

        InternalRootScript(const std::string &name, const StructMetadata &metadata) : InternalScriptBase(metadata) {
            GetScriptDefinitions().RegisterScript(
                {name, {}, false, this, ScriptInitFunc(&Init), OnTickFunc(&OnTick), false});
        }

        template<typename... Events>
        InternalRootScript(const std::string &name,
            const StructMetadata &metadata,
            bool filterOnEvent,
            Events... events)
            : InternalScriptBase(metadata) {
            GetScriptDefinitions().RegisterScript(
                {name, {events...}, filterOnEvent, this, ScriptInitFunc(&Init), OnTickFunc(&OnTick), false});
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

        static void Init(ScriptState &state) {
            T *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            if constexpr (script_has_init_func<T>()) ptr->Init(state);
        }

        static void OnPhysicsUpdate(ScriptState &state,
            Lock<PhysicsUpdateLock> lock,
            Entity ent,
            chrono_clock::duration interval) {
            T *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            ptr->OnPhysicsUpdate(state, lock, ent, interval);
        }

        InternalPhysicsScript(const std::string &name, const StructMetadata &metadata) : InternalScriptBase(metadata) {
            GetScriptDefinitions().RegisterScript(
                {name, {}, false, this, ScriptInitFunc(&Init), OnPhysicsUpdateFunc(&OnPhysicsUpdate), true});
        }

        template<typename... Events>
        InternalPhysicsScript(const std::string &name,
            const StructMetadata &metadata,
            bool filterOnEvent,
            Events... events)
            : InternalScriptBase(metadata) {
            GetScriptDefinitions().RegisterScript({name,
                {events...},
                filterOnEvent,
                this,
                ScriptInitFunc(&Init),
                OnPhysicsUpdateFunc(&OnPhysicsUpdate),
                true});
        }
    };

    template<typename T>
    struct InternalRootPhysicsScript final : public InternalScriptBase {
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

        static void Init(ScriptState &state) {
            T *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            if constexpr (script_has_init_func<T>()) ptr->Init(state);
        }

        static void OnPhysicsUpdate(ScriptState &state,
            PhysicsUpdateLock lock,
            const Entity &ent,
            chrono_clock::duration interval) {
            T *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            ptr->OnPhysicsUpdate(state, lock, ent, interval);
        }

        InternalRootPhysicsScript(const std::string &name, const StructMetadata &metadata)
            : InternalScriptBase(metadata) {
            GetScriptDefinitions().RegisterScript(
                {name, {}, false, this, ScriptInitFunc(&Init), OnPhysicsUpdateFunc(&OnPhysicsUpdate), false});
        }

        template<typename... Events>
        InternalRootPhysicsScript(const std::string &name,
            const StructMetadata &metadata,
            bool filterOnEvent,
            Events... events)
            : InternalScriptBase(metadata) {
            GetScriptDefinitions().RegisterScript({name,
                {events...},
                filterOnEvent,
                this,
                ScriptInitFunc(&Init),
                OnPhysicsUpdateFunc(&OnPhysicsUpdate),
                false});
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
            GetScriptDefinitions().RegisterPrefab({name, {}, false, this, {}, PrefabFunc(&Prefab), false});
        }
    };
} // namespace ecs
