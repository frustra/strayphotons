#include "core/Logging.hh"
#include "graphics/GraphicsManager.hh"
#include "graphics/Renderer.hh"

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

	GraphicsManager::GraphicsManager() : context(nullptr)
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

		lastFrameEnd = std::chrono::high_resolution_clock::now();
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
			context = new Renderer();
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

		auto frameEnd = std::chrono::high_resolution_clock::now();
		fpsTimer += std::chrono::duration<double, std::milli>(frameEnd - lastFrameEnd).count();
		frameCounter++;

		if (fpsTimer > 1000) {
			context->SetTitle("STRAY PHOTONS (" + std::to_string(frameCounter) + " FPS)");
			frameCounter = 0;
			fpsTimer = 0;
		}

		lastFrameEnd = frameEnd;
		glfwPollEvents();
		return true;
	}
}
