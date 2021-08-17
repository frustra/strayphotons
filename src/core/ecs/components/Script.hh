#pragma once

#include <ecs/Components.hh>
#include <ecs/Ecs.hh>
#include <functional>
#include <robin_hood.h>
#include <variant>
#include <vector>

namespace ecs {
    class Script {
    public:
        void AddOnTick(std::function<void(double)> callback) {
            onTickCallbacks.emplace_back(callback);
        }

        void OnTick(double dtSinceLastFrame) {
            for (auto &callback : onTickCallbacks) {
                callback(dtSinceLastFrame);
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
        std::vector<std::function<void(double)>> onTickCallbacks;

        robin_hood::unordered_flat_map<std::string, ParameterType> scriptParameters;
    };

    static Component<Script> ComponentScript("script");

    template<>
    bool Component<Script>::LoadEntity(Lock<AddRemove> lock, Tecs::Entity &dst, const picojson::value &src);
} // namespace ecs
