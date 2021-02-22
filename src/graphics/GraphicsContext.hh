#pragma once

#include "Common.hh"
#include "ecs/components/View.hh"

#include <string>

namespace sp {
    class Game;

    class GraphicsContext {
    public:
        GraphicsContext(Game *game);
        virtual ~GraphicsContext();

        virtual void Init() = 0;
        virtual bool ShouldClose() = 0;
        virtual void BeginFrame() = 0;
        virtual void SwapBuffers() = 0;
        virtual void PopulatePancakeView(ecs::View& view) = 0;
        virtual void PrepareForView(ecs::View& view) = 0;
        virtual void EndFrame() = 0;

        // These functions are acceptable in the base GraphicsContext class, 
        // but really shouldn't needed. They should be replaced with a generic "Settings" API
        // that allows modules to populate a Settings / Options menu entry
        //
        // TODO: default implementation for these functions for graphics contexts that don't support
        // monitor modes
        virtual const vector<glm::ivec2> &MonitorModes() = 0;
        virtual const glm::ivec2 CurrentMode() = 0;

    protected:
        Game *game;
    };
} // namespace sp
