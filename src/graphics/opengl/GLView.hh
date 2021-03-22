#pragma once

#include "graphics/Graphics.hh"
#include "ecs/components/View.hh"

namespace sp {

    static GLbitfield getClearMode(const ecs::View& view) {
        GLbitfield clearMode = 0;
        if (view.clearMode[ecs::View::ClearMode::CLEAR_MODE_COLOR_BUFFER]) {
            clearMode |= GL_COLOR_BUFFER_BIT;
        }

        if (view.clearMode[ecs::View::ClearMode::CLEAR_MODE_DEPTH_BUFFER]) {
            clearMode |= GL_DEPTH_BUFFER_BIT;
        }

        if (view.clearMode[ecs::View::ClearMode::CLEAR_MODE_ACCUMULATION_BUFFER]) {
            clearMode |= GL_ACCUM_BUFFER_BIT;
        }

        if (view.clearMode[ecs::View::ClearMode::CLEAR_MODE_STENCIL_BUFFER]) {
            clearMode |= GL_STENCIL_BUFFER_BIT;
        }

        return clearMode;
    }
};