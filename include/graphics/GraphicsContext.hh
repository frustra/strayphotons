//=== Copyright Frustra Software, all rights reserved ===//

#ifndef SP_GRAPHICSCONTEXT_H
#define SP_GRAPHICSCONTEXT_H

#include "graphics/Graphics.hh"

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
		GLFWwindow *window = NULL;
		GLFWmonitor *monitor = NULL;

		vk::Instance vkinstance;
		vk::SurfaceKHR vksurface;
		vk::Device vkdevice;
		vk::AllocationCallbacks *allocator = NULL;
	};
}

#endif

