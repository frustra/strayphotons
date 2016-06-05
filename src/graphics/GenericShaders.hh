#pragma once

#include "graphics/Shader.hh"

namespace sp
{
	class BasicPostVS : public Shader
	{
		SHADER_TYPE(BasicPostVS)
		using Shader::Shader;
	};

	class ScreenCoverFS : public Shader
	{
		SHADER_TYPE(ScreenCoverFS)
		using Shader::Shader;
	};
}