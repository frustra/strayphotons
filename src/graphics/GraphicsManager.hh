#pragma once

#include "ecs/Ecs.hh"
#include "ecs/components/View.hh"

#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace sp {
    class Game;
    class GlfwGraphicsContext;
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

        GlfwGraphicsContext *GetContext();

        void DisableCursor();
        void EnableCursor();

        Renderer *GetRenderer() {
            return renderer;
        }

    private:
        bool useBasic = false;

        GlfwGraphicsContext *context = nullptr;
        GlfwActionSource *glfwActionSource = nullptr;
        Renderer *renderer = nullptr;
        Game *game = nullptr;
        ProfilerGui *profilerGui = nullptr;

        ecs::Observer<ecs::Removed<ecs::View>> viewRemoval;
        std::vector<ecs::Entity> playerViews;
    };
} // namespace sp
