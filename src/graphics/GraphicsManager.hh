#pragma once

#include "graphics/GraphicsContext.hh"
#include "game/InputManager.hh"
#include "ecs/Ecs.hh"

namespace sp
{
	class Game;

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
		void SetPlayerView(Entity entity);

		bool Frame();

	private:
		ECS::View &updateViewCaches(Entity entity);
		void validateView(Entity viewEntity);

		GraphicsContext *context;
		Game *game;

		Entity playerView;

		double lastFrameEnd = 0, fpsTimer = 0;
		int frameCounter = 0;
	};
}
