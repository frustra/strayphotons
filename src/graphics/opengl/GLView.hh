#pragma once

#include "graphics/Graphics.hh"
#include "ecs/components/View.hh"

namespace sp {

    static GLbitfield getClearMode(const ecs::View& view) {
        GLbitfield clearMode = 0;
        if (view.HasClearMode(ecs::View::ClearMode::ColorBuffer)) {
            clearMode |= GL_COLOR_BUFFER_BIT;
        }

        if (view.HasClearMode(ecs::View::ClearMode::DepthBuffer)) {
            clearMode |= GL_DEPTH_BUFFER_BIT;
        }

        if (view.HasClearMode(ecs::View::ClearMode::AccumulationBuffer)) {
            clearMode |= GL_ACCUM_BUFFER_BIT;
        }

        if (view.HasClearMode(ecs::View::ClearMode::StencilBuffer)) {
            clearMode |= GL_STENCIL_BUFFER_BIT;
        }

        return clearMode;
    }
};