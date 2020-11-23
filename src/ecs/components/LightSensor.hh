#pragma once

#include "Common.hh"

#include <ecs/Components.hh>
#include <ecs/NamedEntity.hh>
#include <glm/glm.hpp>

namespace ecs {
    class LightSensor {
    public:
        struct Trigger {
            glm::vec3 illuminance;
            string oncmd, offcmd;
            float onSignal = 1.0f;
            float offSignal = 0.0f;

            bool operator()(glm::vec3 val) {
                return glm::all(glm::greaterThanEqual(val, illuminance));
            }
        };

        // Required parameters.
        glm::vec3 position = {0, 0, 0}; // In model space.
        glm::vec3 direction = {0, 0, -1}; // In model space.
        glm::vec3 onColor = {0, 1, 0};
        glm::vec3 offColor = {0, 0, 0};

        vector<Trigger> triggers;
        vector<NamedEntity> outputTo;

        // Updated automatically.
        glm::vec3 illuminance;
        bool triggered = false;

        LightSensor() {}
        LightSensor(glm::vec3 p, glm::vec3 n) : position(p), direction(n) {}
    };

    static Component<LightSensor> ComponentLightSensor("lightsensor"); // TODO: rename this

    template<>
    bool Component<LightSensor>::Load(LightSensor &dst, const picojson::value &src);
} // namespace ecs
