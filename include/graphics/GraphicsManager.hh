#ifndef SP_GRAPHICSMANAGER_H
#define SP_GRAPHICSMANAGER_H

#include "graphics/GraphicsContext.hh"

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
	};
}

#endif
