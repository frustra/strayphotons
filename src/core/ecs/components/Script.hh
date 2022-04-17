#pragma once

#include "core/Tracing.hh"
#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"

#include <any>
#include <functional>
#include <robin_hood.h>
#include <variant>
#include <vector>

namespace ecs {
    class ScriptState;

    using OnTickFunc = std::function<void(ScriptState &, Lock<WriteAll>, Entity, chrono_clock::duration)>;
    using PrefabFunc = std::function<void(ScriptState &, Lock<AddRemove>, Entity)>;

    class ScriptState {
    public:
        using ParameterType = typename std::variant<bool,
            double,
            std::string,
            Entity,
            EntityRef,
            std::vector<bool>,
            std::vector<double>,
            std::vector<std::string>>;

        ScriptState() : callback(std::monostate()) {}
        ScriptState(const EntityScope &scope) : scope(scope), callback(std::monostate()) {}
        ScriptState(const EntityScope &scope, OnTickFunc callback) : scope(scope), callback(callback) {}
        ScriptState(const EntityScope &scope, PrefabFunc callback) : scope(scope), callback(callback) {}

        template<typename T>
        void SetParam(std::string name, const T &value) {
            parameters[name] = value;
        }

        template<typename T>
        T GetParam(std::string name) const {
            auto itr = parameters.find(name);
            if (itr == parameters.end()) {
                return T();
            } else {
                return std::get<T>(itr->second);
            }
        }

        operator bool() const {
            return !std::holds_alternative<std::monostate>(callback);
        }

        EntityScope scope;
        std::variant<std::monostate, OnTickFunc, PrefabFunc> callback;
        std::vector<std::string> filterEvents;

        std::any userData;

    private:
        robin_hood::unordered_flat_map<std::string, ParameterType> parameters;

        friend struct Script;
    };

    struct Script {
        ScriptState &AddOnTick(const EntityScope &scope, OnTickFunc callback) {
            return scripts.emplace_back(scope, callback);
        }

        ScriptState &AddPrefab(const EntityScope &scope, PrefabFunc callback) {
            return scripts.emplace_back(scope, callback);
        }

        void OnTick(Lock<WriteAll> lock, const Entity &ent, chrono_clock::duration interval);
        void Prefab(Lock<AddRemove> lock, const Entity &ent);

        std::vector<ScriptState> scripts;
    };

    static Component<Script> ComponentScript("script");

    template<>
    bool Component<Script>::Load(const EntityScope &scope, Script &dst, const picojson::value &src);
    template<>
    void Component<Script>::Apply(const Script &src, Lock<AddRemove> lock, Entity dst);

    extern robin_hood::unordered_node_map<std::string, OnTickFunc> ScriptDefinitions;
    extern robin_hood::unordered_node_map<std::string, PrefabFunc> PrefabDefinitions;

    class InternalScript {
    public:
        InternalScript(const std::string &name, OnTickFunc &&func) {
            ScriptDefinitions[name] = std::move(func);
        }
    };

    class InternalPrefab {
    public:
        InternalPrefab(const std::string &name, PrefabFunc &&func) {
            PrefabDefinitions[name] = std::move(func);
        }
    };
} // namespace ecs
