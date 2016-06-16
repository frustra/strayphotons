#pragma once

#include "graphics/Graphics.hh"
#include "ecs/Entity.hh"
#include "ecs/components/View.hh"

namespace sp
{
	class Game;
	class GuiManager;
	class GuiRenderer;
	class Renderer;
	class InputManager;

	class GraphicsManager
	{
	public:
		GraphicsManager(Game *game);
		~GraphicsManager();

		void CreateContext();
		void ReleaseContext();
		void ReloadContext();
		bool HasActiveContext();

		void BindContextInputCallbacks(InputManager &inputManager);
		void SetPlayerView(ecs::Entity entity);

		bool Frame();

	private:
		ecs::View &updateViewCaches(ecs::Entity entity);
		void validateView(ecs::Entity viewEntity);

		Renderer *renderer = nullptr;
		Game *game = nullptr;
		GuiRenderer *guiRenderer = nullptr;

		ecs::Entity playerView;

		double lastFrameEnd = 0, fpsTimer = 0;
		int frameCounter = 0;
	};
}
