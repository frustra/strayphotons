#include "core/Game.hh"
#include "core/Logging.hh"
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
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);

		shaderSet = new ShaderSet();
	}

	GraphicsContext::~GraphicsContext()
	{
		delete shaderSet;

		if (window)
			glfwDestroyWindow(window);
	}

	void GraphicsContext::CreateWindow()
	{
		// Create window and surface
		window = glfwCreateWindow(1280, 720, "STRAY PHOTONS", nullptr, nullptr);
		Assert(window, "glfw window creation failed");

		glfwMakeContextCurrent(window);

		glewExperimental = GL_TRUE;
		Assert(glewInit() == GLEW_OK, "glewInit failed");
		glGetError();

		Assert(GLEW_ARB_compute_shader, "ARB_compute_shader required");
		Assert(GLEW_ARB_direct_state_access, "ARB_direct_state_access required");

		glDebugMessageCallback(DebugCallback, nullptr);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

		float maxAnisotropy;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
		Debugf("maximum anisotropy: %f", maxAnisotropy);


		Prepare();
	}

	void GraphicsContext::SetTitle(string title)
	{
		glfwSetWindowTitle(window, title.c_str());
	}

	void GraphicsContext::ResetSwapchain(uint32 &width, uint32 &height)
	{

	}

	bool GraphicsContext::ShouldClose()
	{
		return !!glfwWindowShouldClose(window);
	}
}
