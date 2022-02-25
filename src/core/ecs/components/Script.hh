#pragma once

#include "core/Tracing.hh"
#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/NamedEntity.hh"

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
            NamedEntity,
            std::vector<bool>,
            std::vector<double>,
            std::vector<std::string>>;

        ScriptState() : callback(std::monostate()) {}
        ScriptState(OnTickFunc callback)
            : callback(callback) {}
        ScriptState(PrefabFunc callback)
            : callback(callback) {}

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

    private:
        std::variant<std::monostate, OnTickFunc, PrefabFunc> callback;
        robin_hood::unordered_flat_map<std::string, ParameterType> parameters;

        friend struct Script;
    };

    struct Script {
        ScriptState &AddOnTick(OnTickFunc callback) {
            scripts.emplace_back(callback);
        }

        ScriptState &AddPrefab(PrefabFunc callback) {
            scripts.emplace_back(callback);
        }

        void OnTick(Lock<WriteAll> lock, const Entity &ent, chrono_clock::duration interval) {
            ZoneScopedN("OnTick");
            ZoneValue(ent.index);
            for (auto &state : scripts) {
                auto callback = std::get_if<OnTickFunc>(&state.callback);
                if (callback) (*callback)(state, lock, ent, interval);
            }
        }

        void Prefab(Lock<AddRemove> lock, const Entity &ent) {
            ZoneScopedN("Prefab");
            ZoneValue(ent.index);
            for (auto &state : scripts) {
                auto callback = std::get_if<PrefabFunc>(&state.callback);
                if (callback) (*callback)(state, lock, ent);
            }
        }

        std::vector<ScriptState> scripts;
    };

    static Component<Script> ComponentScript("script");

    extern robin_hood::unordered_node_map<std::string, OnTickFunc> ScriptDefinitions;
    extern robin_hood::unordered_node_map<std::string, PrefabFunc> PrefabDefinitions;

    template<>
    bool Component<Script>::Load(sp::Scene *scene, Script &dst, const picojson::value &src);
    template<>
    void Component<Script>::Apply(const Script &src, Lock<AddRemove> lock, Entity dst);
} // namespace ecs
