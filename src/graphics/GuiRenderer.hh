#pragma once

#include "VertexBuffer.hh"
#include "Texture.hh"
#include "ecs/components/View.hh"

namespace sp
{
	class Renderer;
	class GuiManager;

	class GuiRenderer
	{
	public:
		GuiRenderer(Renderer &renderer, GuiManager &manager);
		void Render(ECS::View view);

	private:
		VertexBuffer vertices, indices;
		Texture fontTex;
		double lastTime = 0.0;

		Renderer &parent;
		GuiManager &manager;
	};
}