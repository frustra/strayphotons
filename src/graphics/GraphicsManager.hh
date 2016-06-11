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
		void SetPlayerView(sp::Entity entity);

		bool Loop();
		bool Frame();

	private:
		glm::mat4 getPlayerViewTransform();
		glm::mat4 getPlayerProjectTransform();
		void validateView(Entity viewEntity);

		GraphicsContext *context;
		Game *game;

		Entity playerView;

		double lastFrameEnd = 0, fpsTimer = 0;
		int frameCounter = 0;
	};
}
