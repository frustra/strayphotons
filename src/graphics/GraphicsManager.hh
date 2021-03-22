#pragma once

#include "core/CVar.hh"

#include "ecs/Ecs.hh"
#include "ecs/components/View.hh"

#include "game/gui/ProfilerGui.hh"

namespace sp {
    class Game;
    class GuiRenderer;
    class GlfwGraphicsContext;
    class Renderer;

    extern CVar<glm::ivec2> CVarWindowSize;
    extern CVar<float> CVarFieldOfView;
    extern CVar<int> CVarWindowFullscreen;

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

        GlfwGraphicsContext *GetContext() {
            return context;
        }

        Renderer* GetRenderer() {
            return renderer;
        }

    private:
        bool useBasic = false;

        GlfwGraphicsContext* context = nullptr;
        Renderer* renderer = nullptr;
        Game* game = nullptr;
        ProfilerGui* profilerGui = nullptr;

        ecs::Observer<ecs::Removed<ecs::View>> viewRemoval;
        vector<ecs::Entity> playerViews;
        
    };
} // namespace sp
