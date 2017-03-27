#pragma once

#include "graphics/Graphics.hh"
#include "ecs/Entity.hh"
#include "ecs/components/View.hh"
#include "game/gui/ProfilerGui.hh"
#include "core/CVar.hh"

namespace sp
{
	class Game;
	class GuiRenderer;
	class GraphicsContext;
	class InputManager;

	extern CVar<glm::ivec2> CVarWindowSize;
	extern CVar<float> CVarFieldOfView;
	extern CVar<int> CVarWindowFullscreen;

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
		void SetPlayerView(vector<ecs::Entity> entities);
		void RenderLoading();

		bool Frame();

		GraphicsContext *GetContext()
		{
			return context;
		}

	private:
		bool useBasic = false;

		GraphicsContext *context = nullptr;
		Game *game = nullptr;
		ProfilerGui *profilerGui = nullptr;

#ifdef SP_ENABLE_RAYTRACER
		raytrace::RaytracedRenderer *rayTracer = nullptr;
#endif

		vector<ecs::Entity> playerViews;

		double lastFrameEnd = 0, fpsTimer = 0;
		int frameCounter = 0;
	};
}
