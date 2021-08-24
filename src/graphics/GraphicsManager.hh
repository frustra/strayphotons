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

    #ifdef SP_GRAPHICS_SUPPORT_GL
    class VoxelRenderer;
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_VK
    namespace vulkan {
        class Renderer;
    }
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

    private:
        GraphicsContext *context = nullptr;
        Game *game = nullptr;

    #ifdef SP_GRAPHICS_SUPPORT_GL
        VoxelRenderer *renderer = nullptr;
        std::shared_ptr<ProfilerGui> profilerGui;
        PerfTimer timer;
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_VK
        vulkan::Renderer *renderer = nullptr;
    #endif
    };
} // namespace sp

#endif
