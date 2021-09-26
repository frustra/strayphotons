#pragma once

#include <ecs/Components.hh>
#include <ecs/Ecs.hh>
#include <functional>
#include <robin_hood.h>
#include <variant>
#include <vector>

namespace ecs {
    using ScriptFunc = std::function<void(ecs::Lock<ecs::WriteAll>, Tecs::Entity, double)>;
    class Script {
    public:
        void AddOnTick(ScriptFunc callback) {
            onTickCallbacks.push_back(callback);
        }

        void OnTick(ecs::Lock<ecs::WriteAll> lock, Tecs::Entity &ent, double dtSinceLastFrame) {
            for (auto &callback : onTickCallbacks) {
                callback(lock, ent, dtSinceLastFrame);
            }
        }

        using ParameterType = typename std::variant<bool, double, std::string>;

        template<typename T>
        void SetParam(std::string name, T &&value) {
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

        robin_hood::unordered_flat_map<std::string, ParameterType> scriptParameters;
    };

    static Component<Script> ComponentScript("script");

    extern robin_hood::unordered_node_map<std::string, ScriptFunc> ScriptDefinitions;

    template<>
    bool Component<Script>::Load(Lock<Read<ecs::Name>> lock, Script &dst, const picojson::value &src);
} // namespace ecs
