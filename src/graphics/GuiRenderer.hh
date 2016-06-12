#pragma once

#include "VertexBuffer.hh"
#include "Texture.hh"
#include "ecs/components/View.hh"

namespace sp
{
	class Renderer;

	class GuiRenderer
	{
	public:
		GuiRenderer(Renderer *renderer);
		void Render(ECS::View view);

	private:
		void DefineWindows();

		VertexBuffer vertices, indices;
		Texture fontTex;
		double lastTime = 0.0;

		Renderer *parent;
	};
}