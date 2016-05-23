#ifndef SP_GRAPHICSMANAGER_H
#define SP_GRAPHICSMANAGER_H

#include "graphics/GraphicsContext.hh"

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

		bool Loop();
		bool Frame();

	private:
		GraphicsContext *context;
		Game *game;

		double lastFrameEnd = 0, fpsTimer = 0;
		int frameCounter = 0;
	};
}

#endif
