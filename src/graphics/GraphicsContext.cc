#include "core/Game.hh"
#include "core/Logging.hh"
#include "graphics/GraphicsContext.hh"
#include "graphics/Shader.hh"

#include <string>
#include <iostream>

namespace sp
{
	GraphicsContext::GraphicsContext()
	{
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

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
