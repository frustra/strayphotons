#pragma once

#include "ecs/Ecs.hh"

#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace sp {
    class Game;
    class GraphicsContext;
    class Renderer;
    class ProfilerGui;
    class GlfwActionSource;

    class GraphicsManager {
    public:
        GraphicsManager(Game *game);
        ~GraphicsManager();

        void Init();
        bool HasActiveContext();

        void AddPlayerView(ecs::Entity entity);
        void AddPlayerView(Tecs::Entity entity);

        void RenderLoading();

        bool Frame();

        GraphicsContext *GetContext();

        void DisableCursor();
        void EnableCursor();

        Renderer *GetRenderer() {
            return renderer;
        }

    private:
        GraphicsContext *context = nullptr;
        Renderer *renderer = nullptr;
        Game *game = nullptr;

#ifdef SP_GRAPHICS_SUPPORT_GL
        bool useBasic = false;
        GlfwActionSource *glfwActionSource = nullptr;
        ProfilerGui *profilerGui = nullptr;
#endif

        ecs::Observer<ecs::Removed<ecs::View>> viewRemoval;
        std::vector<ecs::Entity> playerViews;
    };
} // namespace sp
