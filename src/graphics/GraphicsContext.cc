//=== Copyright Frustra Software, all rights reserved ===//

#include "core/Game.hh"
#include "core/Logging.hh"
#include "graphics/GraphicsContext.hh"

#include <string>
#include <iostream>

namespace sp
{
	GraphicsContext::GraphicsContext() : window(NULL), monitor(NULL)
	{
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		//glfwWindowHint(GLFW_DEPTH_BITS, 24);
	}

	GraphicsContext::~GraphicsContext()
	{
		glfwDestroyWindow(window);
	}

	void GraphicsContext::CreateWindow()
	{
		// if (fullscreen) monitor = glfwGetPrimaryMonitor();

		if (!(window = glfwCreateWindow(1440, 810, "Stray Photons", monitor, NULL)))
		{
			throw "glfw window creation failed";
		}

		int major = VK_API_VERSION >> 22;
		int minor = (VK_API_VERSION >> 12) & 0x3ff;
		int patch = VK_API_VERSION & 0xfff;
		logging::Log("Created context using Vulkan API %d.%d.%d", major, minor, patch);

		glfwSwapInterval(1);
	}

	bool GraphicsContext::ShouldClose()
	{
		return glfwWindowShouldClose(window);
	}

	void GraphicsContext::RenderFrame()
	{
		/* glfwSwapBuffers(window); */
	}
}

