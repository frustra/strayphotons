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
		void SetPlayerView(ECS::Entity entity);

		bool Frame();

	private:
		ECS::View &updateViewCaches(ECS::Entity entity);
		void validateView(ECS::Entity viewEntity);

		Renderer *renderer = nullptr;
		Game *game = nullptr;
		GuiRenderer *guiRenderer = nullptr;

		ECS::Entity playerView;

		double lastFrameEnd = 0, fpsTimer = 0;
		int frameCounter = 0;
	};
}
