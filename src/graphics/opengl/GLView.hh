#pragma once

#include "graphics/Graphics.hh"
#include "ecs/components/View.hh"

namespace sp {

    static GLbitfield getClearMode(const ecs::View& view) {
        GLbitfield clearMode = 0;
        if (view.clearMode.ColorBuffer) {
            clearMode |= GL_COLOR_BUFFER_BIT;
        }

        if (view.clearMode.DepthBuffer) {
            clearMode |= GL_DEPTH_BUFFER_BIT;
        }

        if (view.clearMode.AccumulationBuffer) {
            clearMode |= GL_ACCUM_BUFFER_BIT;
        }

        if (view.clearMode.StencilBuffer) {
            clearMode |= GL_STENCIL_BUFFER_BIT;
        }

        return clearMode;
    }
};