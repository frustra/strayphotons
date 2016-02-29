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
		vk::destroySurfaceKHR(vkinstance, vksurface, allocator);
		vk::destroyInstance(vkinstance, allocator);
		glfwDestroyWindow(window);
	}

	void GraphicsContext::CreateWindow()
	{
		const char *layers[] =
		{
			"VK_LAYER_LUNARG_standard_validation",
		};

		int nExts;
		const char **extensions = glfwGetRequiredInstanceExtensions(&nExts);

		vk::ApplicationInfo appInfo("Stray Photons", 1, "SP ENGINE", 1, VK_API_VERSION);

		vk::InstanceCreateInfo info;
		info.pApplicationInfo(&appInfo);
		info.enabledLayerCount(sizeof(layers) / sizeof(*layers));
		info.ppEnabledLayerNames(layers);
		info.enabledExtensionCount(nExts);
		info.ppEnabledExtensionNames(extensions);

		// Initialize Vulkan
		vk::Assert(vk::createInstance(&info, allocator, &vkinstance), "creating Vulkan instance");

		int major = VK_API_VERSION >> 22;
		int minor = (VK_API_VERSION >> 12) & 0x3ff;
		int patch = VK_API_VERSION & 0xfff;
		Logf("Created Vulkan instance with API v%d.%d.%d", major, minor, patch);

		// Create window and surface
		if (!(window = glfwCreateWindow(1440, 810, "Stray Photons", monitor, NULL)))
		{
			throw "glfw window creation failed";
		}

		auto result = glfwCreateWindowSurface((VkInstance)vkinstance, window, (VkAllocationCallbacks *)allocator, ((VkSurfaceKHR *)&vksurface));
		vk::Assert(result, "creating window surface");

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

