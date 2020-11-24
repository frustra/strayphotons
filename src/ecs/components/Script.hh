#pragma once

#include <ecs/Components.hh>
#include <ecs/Ecs.hh>
#include <functional>
#include <robin_hood.h>
#include <vector>

namespace ecs {
    class Script {
    public:
        void AddOnTick(std::function<void(ECS &, double)> callback) {
            onTickCallbacks.emplace_back(callback);
        }

        void OnTick(double dtSinceLastFrame, ECS &ecs) {
            for (auto &callback : onTickCallbacks) {
                callback(ecs, dtSinceLastFrame);
            }
        }

        void SetParam(std::string name, double value) {
            scriptParameters[name] = value;
        }

        double GetParam(std::string name) const {
            auto itr = scriptParameters.find(name);
            if (itr == scriptParameters.end()) {
                return 0.0;
            } else {
                return itr->second;
            }
        }

    private:
        std::vector<std::function<void(ECS &, double)>> onTickCallbacks;
        robin_hood::unordered_flat_map<std::string, double> scriptParameters;
    };

    static Component<Script> ComponentScript("script");

    template<>
    bool Component<Script>::LoadEntity(Lock<AddRemove> lock, Tecs::Entity &dst, const picojson::value &src);
} // namespace ecs
