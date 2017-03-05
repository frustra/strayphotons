#pragma once

#include "VertexBuffer.hh"
#include "Texture.hh"
#include "ecs/components/View.hh"

class ImGuiContext;

namespace sp
{
	class Renderer;
	class GuiManager;

	class GuiRenderer
	{
	public:
		GuiRenderer(Renderer &renderer, GuiManager &manager, string font);
		~GuiRenderer();
		void Render(ecs::View view);

	private:
		VertexBuffer vertices, indices;
		Texture fontTex;
		double lastTime = 0.0;

		Renderer &parent;
		GuiManager &manager;
		ImGuiContext *imCtx = nullptr;
	};
}
