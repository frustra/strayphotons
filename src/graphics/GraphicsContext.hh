#pragma once

#include <string>

#include "graphics/Graphics.hh"
#include "graphics/RenderTarget.hh"
#include "ecs/components/View.hh"
#include "Common.hh"
#include <core/PerfTimer.hh>

struct GLFWwindow;

namespace sp
{
	class Device;
	class ShaderSet;
	class Game;
	class GlfwInputManager;
	class RenderTarget;

	class GraphicsContext
	{
	public:
		GraphicsContext(Game *game);
		virtual ~GraphicsContext();

		void CreateWindow(glm::ivec2 initialSize = { 640, 480 });
		bool ShouldClose();
		void SetTitle(string title);
		void BindInputCallbacks(GlfwInputManager *inputManager);
		void ResizeWindow(ecs::View &frameBufferView, double scale, int fullscreen);
		const vector<glm::ivec2> &MonitorModes();
		const glm::ivec2 CurrentMode();

		virtual void Prepare() = 0;
		virtual void BeginFrame() = 0;
		virtual void RenderPass(ecs::View view, RenderTarget::Ref finalOutput = nullptr) = 0;
		virtual void PrepareForView(const ecs::View &view) = 0;
		virtual void RenderLoading(ecs::View view) = 0;
		virtual void EndFrame() = 0;

		ShaderSet *GlobalShaders;
		PerfTimer Timer;

		GLFWwindow *GetWindow()
		{
			return window;
		}

	private:
		glm::ivec2 prevWindowSize, prevWindowPos;
		int prevFullscreen = 0;
		double windowScale = 1.0;
		vector<glm::ivec2> monitorModes;

	protected:
		GLFWwindow *window = nullptr;
		Game *game;
		GlfwInputManager *input = nullptr;
	};
}
