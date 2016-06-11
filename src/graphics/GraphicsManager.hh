#pragma once

#include "graphics/GraphicsContext.hh"
#include "game/InputManager.hh"

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

		bool Loop();
		bool Frame();

	private:
		GraphicsContext *context;
		Game *game;

		double lastFrameEnd = 0, fpsTimer = 0;
		int frameCounter = 0;
	};
}
