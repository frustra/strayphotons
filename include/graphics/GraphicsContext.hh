//=== Copyright Frustra Software, all rights reserved ===//

#ifndef SP_GRAPHICSCONTEXT_H
#define SP_GRAPHICSCONTEXT_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace sp
{
	class GraphicsContext
	{
	public:
		GraphicsContext();
		~GraphicsContext();

		void CreateWindow();
		bool ShouldClose();
		void RenderFrame();

		//Viewport view;

	private:
		GLFWwindow *window;
		GLFWmonitor *monitor;
	};
}

#endif

