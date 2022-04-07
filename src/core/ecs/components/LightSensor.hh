#pragma once

#include <ecs/Components.hh>
#include <ecs/Ecs.hh>
#include <glm/glm.hpp>

namespace ecs {
    class LightSensor {
    public:
        // Required parameters.
        glm::vec3 position = {0, 0, 0}; // In model space.
        glm::vec3 direction = {0, 0, -1}; // In model space.

        // Updated automatically.
        glm::vec3 illuminance;

        LightSensor() {}
        LightSensor(glm::vec3 p, glm::vec3 n) : position(p), direction(n) {}
    };

    static Component<LightSensor> ComponentLightSensor("light_sensor");

    template<>
    bool Component<LightSensor>::Load(ScenePtr scenePtr,
        const Name &scope,
        LightSensor &dst,
        const picojson::value &src);
} // namespace ecs
