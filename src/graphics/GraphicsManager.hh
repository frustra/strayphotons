#pragma once

#include "core/CVar.hh"
#include "ecs/Entity.hh"
#include "ecs/components/View.hh"
#include "game/gui/ProfilerGui.hh"
#include "graphics/Graphics.hh"

namespace sp {
	class Game;
	class GuiRenderer;
	class GraphicsContext;
	class GlfwInputManager;

	extern CVar<glm::ivec2> CVarWindowSize;
	extern CVar<float> CVarFieldOfView;
	extern CVar<int> CVarWindowFullscreen;

	class GraphicsManager {
	public:
		GraphicsManager(Game *game);
		~GraphicsManager();

		void CreateContext();
		void ReleaseContext();
		void ReloadContext();
		bool HasActiveContext();

		void BindContextInputCallbacks(GlfwInputManager *inputManager);
		void SetPlayerView(vector<ecs::Entity> entities);
		void RenderLoading();

		bool Frame();

		GraphicsContext *GetContext() {
			return context;
		}

	private:
		bool useBasic = false;

		GraphicsContext *context = nullptr;
		Game *game = nullptr;
		ProfilerGui *profilerGui = nullptr;

		vector<ecs::Entity> playerViews;

		double lastFrameEnd = 0, fpsTimer = 0;
		int frameCounter = 0;
	};
} // namespace sp
