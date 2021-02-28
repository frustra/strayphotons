#pragma once

#include "graphics/Graphics.hh"
#include "ecs/components/View.hh"

namespace sp {

    static GLbitfield getClearMode(const ecs::View& view) {
        GLbitfield clearMode = 0;
        if (ecs::ClearMode::hasClearMode(view.clearMode, ecs::ClearMode::ColorBuffer)) {
            clearMode |= GL_COLOR_BUFFER_BIT;
        }

        if (ecs::ClearMode::hasClearMode(view.clearMode, ecs::ClearMode::DepthBuffer)) {
            clearMode |= GL_DEPTH_BUFFER_BIT;
        }

        if (ecs::ClearMode::hasClearMode(view.clearMode, ecs::ClearMode::AccumulationBuffer)) {
            clearMode |= GL_ACCUM_BUFFER_BIT;
        }

        if (ecs::ClearMode::hasClearMode(view.clearMode, ecs::ClearMode::StencilBuffer)) {
            clearMode |= GL_STENCIL_BUFFER_BIT;
        }

        return clearMode;
    }
};