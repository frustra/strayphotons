#include "core/Game.hh"
#include "core/Logging.hh"
#include "graphics/GPUTimer.hh"
#include "graphics/GraphicsContext.hh"
#include "graphics/Shader.hh"

#include <string>
#include <iostream>

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
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);

		GlobalShaders = new ShaderSet();
		Timer = new GPUTimer();
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

		if (GLEW_KHR_debug)
		{
			glDebugMessageCallback(DebugCallback, nullptr);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
		}

		float maxAnisotropy;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
		Debugf("maximum anisotropy: %f", maxAnisotropy);

		Prepare();
	}

	void GraphicsContext::SetTitle(string title)
	{
		glfwSetWindowTitle(window, title.c_str());
	}

	void GraphicsContext::BindInputCallbacks(InputManager &inputManager)
	{
		inputManager.BindCallbacks(window);
	}

	bool GraphicsContext::ShouldClose()
	{
		return !!glfwWindowShouldClose(window);
	}
}
