//=== Copyright Frustra Software, all rights reserved ===//

#include "core/Logging.hh"
#include "graphics/GraphicsManager.hh"

#include <iostream>

namespace sp
{
	static void glfwErrorCallback(int error, const char *message)
	{
		Errorf("GLFW returned %d: %s", error, message);
	}

	static void handleVulkanError(vk::Exception &err)
	{
		auto str = vk::getString(err.result());
		Errorf("Vulkan error %s (%d) %s", str.c_str(), err.result(), err.what());
		throw "Vulkan exception";
	}

	GraphicsManager::GraphicsManager() : context(NULL)
	{
		Logf("Graphics starting up");

		glfwSetErrorCallback(glfwErrorCallback);

		if (!glfwInit())
		{
			throw "glfw failed";
		}

		if (!glfwVulkanSupported())
		{
			throw "no compatible Vulkan ICD found";
		}
	}

	GraphicsManager::~GraphicsManager()
	{
		if (context) ReleaseContext();
		glfwTerminate();
	}

	void GraphicsManager::CreateContext()
	{
		if (context) throw "already an active context";

		try
		{
			context = new GraphicsContext();
			context->CreateWindow();
		}
		catch (vk::Exception &err)
		{
			handleVulkanError(err);
		}
	}

	void GraphicsManager::ReleaseContext()
	{
		if (!context) throw "no active context";

		try
		{
			delete context;
		}
		catch (vk::Exception &err)
		{
			handleVulkanError(err);
		}
	}

	void GraphicsManager::ReloadContext()
	{
		//context->Reload();
	}

	bool GraphicsManager::HasActiveContext()
	{
		return context && !context->ShouldClose();
	}

	bool GraphicsManager::Frame()
	{
		if (!context) throw "missing context";
		if (!HasActiveContext()) return false;

		try
		{
			context->RenderFrame();
		}
		catch (vk::Exception &err)
		{
			handleVulkanError(err);
		}

		glfwPollEvents();
		return true;
	}
}

