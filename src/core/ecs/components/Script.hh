#pragma once

#include "core/Tracing.hh"
#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/NamedEntity.hh"

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
            NamedEntity,
            std::vector<bool>,
            std::vector<double>,
            std::vector<std::string>>;

        ScriptState() : callback(std::monostate()) {}
        ScriptState(ScenePtr scene) : scene(scene), callback(std::monostate()) {}
        ScriptState(ScenePtr scene, OnTickFunc callback) : scene(scene), callback(callback) {}
        ScriptState(ScenePtr scene, PrefabFunc callback) : scene(scene), callback(callback) {}

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

        ScenePtr scene;
        std::variant<std::monostate, OnTickFunc, PrefabFunc> callback;

        std::any userData;

    private:
        robin_hood::unordered_flat_map<std::string, ParameterType> parameters;

        friend struct Script;
    };

    struct Script {
        ScriptState &AddOnTick(ScenePtr scene, OnTickFunc callback) {
            return scripts.emplace_back(scene, callback);
        }

        ScriptState &AddPrefab(ScenePtr scene, PrefabFunc callback) {
            return scripts.emplace_back(scene, callback);
        }

        void OnTick(Lock<WriteAll> lock, const Entity &ent, chrono_clock::duration interval) {
            ZoneScopedN("OnTick");
            ZoneStr(ecs::ToString(lock, ent));
            for (auto &state : scripts) {
                auto callback = std::get_if<OnTickFunc>(&state.callback);
                if (callback) (*callback)(state, lock, ent, interval);
            }
        }

        void Prefab(Lock<AddRemove> lock, const Entity &ent) {
            ZoneScopedN("Prefab");
            ZoneStr(ecs::ToString(lock, ent));
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
    bool Component<Script>::Load(ScenePtr scenePtr, Script &dst, const picojson::value &src);
    template<>
    void Component<Script>::Apply(const Script &src, Lock<AddRemove> lock, Entity dst);
} // namespace ecs
