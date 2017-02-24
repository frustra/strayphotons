#pragma once

#include "graphics/Graphics.hh"
#include "ecs/Entity.hh"
#include "ecs/components/View.hh"

namespace sp
{
	class Game;
	class GuiManager;
	class GuiRenderer;
	class GraphicsContext;
	class InputManager;

	namespace raytrace
	{
		class RaytracedRenderer;
	}

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
		bool useBasic = false;

		GraphicsContext *context = nullptr;
		Game *game = nullptr;
		GuiRenderer *guiRenderer = nullptr;

#ifdef SP_ENABLE_RAYTRACER
		raytrace::RaytracedRenderer *rayTracer = nullptr;
#endif

		ecs::Entity playerView;

		double lastFrameEnd = 0, fpsTimer = 0;
		int frameCounter = 0;
	};
}
