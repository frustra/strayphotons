#include "core/Game.hh"
#include "core/Logging.hh"
#include "core/PerfTimer.hh"
#include "graphics/GraphicsContext.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/Shader.hh"
#include <GLFW/glfw3.h>
#include <game/input/GlfwInputManager.hh>

#include <string>
#include <iostream>
#include <algorithm>

namespace sp
{
	static void GLAPIENTRY DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *user)
	{
		if (type == GL_DEBUG_TYPE_OTHER) return;
		Debugf("[GL 0x%X] 0x%X: %s", id, type, message);
	}

	GraphicsContext::GraphicsContext(Game *game) : game(game)
	{
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

		GlobalShaders = new ShaderSet();
	}

	GraphicsContext::~GraphicsContext()
	{
		delete GlobalShaders;

		if (window)
			glfwDestroyWindow(window);
	}

	void GraphicsContext::CreateWindow(glm::ivec2 initialSize)
	{
		// Create window and surface
		window = glfwCreateWindow(initialSize.x, initialSize.y, "STRAY PHOTONS", nullptr, nullptr);
		Assert(window, "glfw window creation failed");

		glfwMakeContextCurrent(window);

		glewExperimental = GL_TRUE;
		Assert(glewInit() == GLEW_OK, "glewInit failed");
		glGetError();

		Logf("OpenGL version: %s", glGetString(GL_VERSION));

		string vendorStr = (char *) glGetString(GL_VENDOR);
		if (starts_with(vendorStr, "NVIDIA"))
		{
			Logf("GPU vendor: NVIDIA");
			ShaderManager::SetDefine("NVIDIA_GPU");
		}
		else if (starts_with(vendorStr, "ATI"))
		{
			Logf("GPU vendor: AMD");
			ShaderManager::SetDefine("AMD_GPU");
		}
		else if (starts_with(vendorStr, "Intel"))
		{
			Logf("GPU vendor: Intel");
			ShaderManager::SetDefine("INTEL_GPU");
		}
		else
		{
			Logf("GPU vendor: Unknown (%s)", vendorStr);
			ShaderManager::SetDefine("UNKNOWN_GPU");
		}

		if (GLEW_KHR_debug)
		{
			glDebugMessageCallback(DebugCallback, nullptr);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
		}

		float maxAnisotropy;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
		Debugf("Maximum anisotropy: %f", maxAnisotropy);

		Prepare();
	}

	void GraphicsContext::SetTitle(string title)
	{
		glfwSetWindowTitle(window, title.c_str());
	}

	void GraphicsContext::BindInputCallbacks(GlfwInputManager *inputManager)
	{
		Assert(input == nullptr, "InputManager must only be bound once.");
		if (inputManager != nullptr)
		{
			input = inputManager;

			input->BindCallbacks(window);
		}
	}

	bool GraphicsContext::ShouldClose()
	{
		return !!glfwWindowShouldClose(window);
	}

	void GraphicsContext::ResizeWindow(ecs::View &view, double scale, int fullscreen)
	{
		glm::ivec2 scaled = glm::dvec2(view.extents) * scale;

		if (prevFullscreen != fullscreen)
		{
			if (fullscreen == 0)
			{
				glfwSetWindowMonitor(window, nullptr, prevWindowPos.x, prevWindowPos.y, scaled.x, scaled.y, 0);
			}
			else if (fullscreen == 1)
			{
				glfwGetWindowPos(window, &prevWindowPos.x, &prevWindowPos.y);
				glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, scaled.x, scaled.y, 60);
			}
		}
		else if (prevWindowSize != view.extents || windowScale != scale)
		{
			if (fullscreen)
			{
				glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, scaled.x, scaled.y, 60);
			}
			else
			{
				glfwSetWindowSize(window, scaled.x, scaled.y);
			}
		}

		prevFullscreen = fullscreen;
		prevWindowSize = view.extents;
		windowScale = scale;
	}

	const vector<glm::ivec2> &GraphicsContext::MonitorModes()
	{
		if (!monitorModes.empty())
			return monitorModes;

		int count;
		const GLFWvidmode *modes = glfwGetVideoModes(glfwGetPrimaryMonitor(), &count);

		for (int i = 0; i < count; i++)
		{
			glm::ivec2 size(modes[i].width, modes[i].height);
			if (std::find(monitorModes.begin(), monitorModes.end(), size) == monitorModes.end())
			{
				monitorModes.push_back(size);
			}
		}

		std::sort(monitorModes.begin(), monitorModes.end(), [](const glm::ivec2 & a, const glm::ivec2 & b)
		{
			return a.x > b.x || (a.x == b.x && a.y > b.y);
		});

		return monitorModes;
	}

	const glm::ivec2 GraphicsContext::CurrentMode()
	{
		const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
		if (mode != NULL)
		{
			return glm::ivec2(mode->width, mode->height);
		}
		return glm::ivec2(0);
	}
}
