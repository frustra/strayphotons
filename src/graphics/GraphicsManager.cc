//=== Copyright Frustra Software, all rights reserved ===//

#include "core/Logging.hh"
#include "graphics/GraphicsManager.hh"

#include <iostream>

namespace sp
{
	static void glfwErrorCallback(int error, const char *message)
	{
		logging::Error("GLFW returned %d: %s", error, message);
	}

	GraphicsManager::GraphicsManager() : context(NULL)
	{
		logging::Log("Graphics starting up");

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
		context = new GraphicsContext();
		context->CreateWindow();
	}

	void GraphicsManager::ReleaseContext()
	{
		if (!context) throw "no active context";
		delete context;
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
		context->RenderFrame();
		glfwPollEvents();
		return true;
	}
}

