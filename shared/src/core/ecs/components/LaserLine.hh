/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
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
        float mediaDensityFactor = 0.6f;
        bool on = true;
        bool relative = true; // multiply transform
        float radius = 0.003; // in world units
    };

    static StructMetadata MetadataLaserLine(typeid(LaserLine),
        "laser_line",
        "",
        StructField::New("intensity", &LaserLine::intensity),
        StructField::New("media_density", &LaserLine::mediaDensityFactor),
        StructField::New("on", &LaserLine::on),
        StructField::New("relative", &LaserLine::relative),
        StructField::New("radius", &LaserLine::radius));
    static Component<LaserLine> ComponentLaserLine(MetadataLaserLine);

    template<>
    void StructMetadata::InitUndefined<LaserLine>(LaserLine &dst);
    template<>
    bool StructMetadata::Load<LaserLine>(LaserLine &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<LaserLine>(const EntityScope &scope,
        picojson::value &dst,
        const LaserLine &src,
        const LaserLine *def);
    template<>
    void Component<LaserLine>::Apply(LaserLine &dst, const LaserLine &src, bool liveTarget);
} // namespace ecs
