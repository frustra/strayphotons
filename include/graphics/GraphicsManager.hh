#ifndef SP_GRAPHICSMANAGER_H
#define SP_GRAPHICSMANAGER_H

#include "graphics/GraphicsContext.hh"

#include <chrono>

namespace sp
{
	class GraphicsManager
	{
	public:
		GraphicsManager();
		~GraphicsManager();

		void CreateContext();
		void ReleaseContext();
		void ReloadContext();
		bool HasActiveContext();

		bool Loop();
		bool Frame();

	private:
		GraphicsContext *context;

		std::chrono::time_point<std::chrono::steady_clock> lastFrameEnd;
		double fpsTimer = 0;
		int frameCounter = 0;
	};
}

#endif
