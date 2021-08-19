#pragma once

#ifdef SP_GRAPHICS_SUPPORT

    #include "ecs/Ecs.hh"

    #ifdef SP_GRAPHICS_SUPPORT_GL
        #include "graphics/opengl/PerfTimer.hh"
    #endif

    #include <glm/glm.hpp>
    #include <memory>
    #include <vector>

namespace sp {
    class Game;
    class GraphicsContext;
    class ProfilerGui;
    class GlfwActionSource;

    #ifdef SP_GRAPHICS_SUPPORT_GL
    class VoxelRenderer;
    #endif

    class GraphicsManager {
    public:
        GraphicsManager(Game *game);
        ~GraphicsManager();

        void Init();
        bool HasActiveContext();

        void RenderLoading();

        bool Frame();

        GraphicsContext *GetContext();
        void DisableCursor();
        void EnableCursor();

    private:
        GraphicsContext *context = nullptr;
        Game *game = nullptr;

    #ifdef SP_GRAPHICS_SUPPORT_GL
        VoxelRenderer *renderer = nullptr;
        GlfwActionSource *glfwActionSource = nullptr;
        ProfilerGui *profilerGui = nullptr;
        PerfTimer timer;
    #endif
    };
} // namespace sp

#endif
