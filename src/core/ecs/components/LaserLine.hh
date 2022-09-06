#pragma once

#include "core/Common.hh"
#include "ecs/Components.hh"
#include "ecs/Ecs.hh"

#include <glm/glm.hpp>
#include <variant>

namespace ecs {
    struct LaserLine {
        struct Line {
            std::vector<glm::vec3> points;
            sp::color_t color = glm::vec3(1); // HDR value
        };
        struct Segment {
            glm::vec3 start, end;
            sp::color_t color = glm::vec3(1); // HDR value
        };
        using Segments = std::vector<Segment>;
        std::variant<Line, Segments> line = Line();

        float intensity = 1.0f; // multiplier applied to color
        float mediaDensityFactor = 1.0f;
        bool on = true;
        bool relative = true; // multiply transform
        float radius = 0.003; // in world units
    };

    static Component<LaserLine> ComponentLaserLine("laser_line",
        ComponentField::New("intensity", &LaserLine::intensity),
        ComponentField::New("media_density", &LaserLine::mediaDensityFactor),
        ComponentField::New("on", &LaserLine::on),
        ComponentField::New("relative", &LaserLine::relative),
        ComponentField::New("radius", &LaserLine::radius));

    template<>
    bool Component<LaserLine>::Load(const EntityScope &scope, LaserLine &dst, const picojson::value &src);
    template<>
    void Component<LaserLine>::Save(Lock<Read<Name>> lock,
        const EntityScope &scope,
        picojson::value &dst,
        const LaserLine &src);
    template<>
    void Component<LaserLine>::Apply(const LaserLine &src, Lock<AddRemove> lock, Entity dst);
} // namespace ecs
