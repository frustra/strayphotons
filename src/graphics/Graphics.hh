#pragma once

#include "Common.hh"

#include <GL/glew.h>

#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace sp
{
	void AssertGLOK(string message);
}
