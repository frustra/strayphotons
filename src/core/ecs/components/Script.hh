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
    using ScriptFunc = std::function<void(ecs::Lock<ecs::WriteAll>, Tecs::Entity, chrono_clock::duration)>;
    using PrefabFunc = std::function<void(ecs::Lock<ecs::AddRemove>, Tecs::Entity)>;

    class Script {
    public:
        void AddOnTick(ScriptFunc callback) {
            onTickCallbacks.push_back(callback);
        }

        void AddPrefab(PrefabFunc callback) {
            prefabCallbacks.push_back(callback);
        }

        void OnTick(ecs::Lock<ecs::WriteAll> lock, const Tecs::Entity &ent, chrono_clock::duration interval) {
            ZoneScopedN("OnTick");
            ZoneValue(ent.index);
            for (auto &callback : onTickCallbacks) {
                callback(lock, ent, interval);
            }
        }

        void Prefab(ecs::Lock<ecs::AddRemove> lock, const Tecs::Entity &ent) {
            ZoneScopedN("Prefab");
            ZoneValue(ent.index);
            for (auto &callback : prefabCallbacks) {
                callback(lock, ent);
            }
        }

        using ParameterType = typename std::variant<bool,
            double,
            std::string,
            Tecs::Entity,
            NamedEntity,
            std::vector<bool>,
            std::vector<double>,
            std::vector<std::string>>;

        void CopyCallbacks(const Script &src);
        void CopyParams(const Script &src);

        template<typename T>
        void SetParam(std::string name, const T &value) {
            scriptParameters[name] = value;
        }

        template<typename T>
        T GetParam(std::string name) const {
            auto itr = scriptParameters.find(name);
            if (itr == scriptParameters.end()) {
                return T();
            } else {
                return std::get<T>(itr->second);
            }
        }

    private:
        std::vector<ScriptFunc> onTickCallbacks;
        std::vector<PrefabFunc> prefabCallbacks;

        robin_hood::unordered_flat_map<std::string, ParameterType> scriptParameters;
    };

    static Component<Script> ComponentScript("script");

    extern robin_hood::unordered_node_map<std::string, ScriptFunc> ScriptDefinitions;
    extern robin_hood::unordered_node_map<std::string, PrefabFunc> PrefabDefinitions;

    template<>
    bool Component<Script>::Load(sp::Scene *scene, Script &dst, const picojson::value &src);
} // namespace ecs
