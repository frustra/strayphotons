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

	class ScreenCoverNoAlphaFS : public Shader
	{
		SHADER_TYPE(ScreenCoverNoAlphaFS)
		using Shader::Shader;
	};

	class BasicOrthoVS : public Shader
	{
		SHADER_TYPE(BasicOrthoVS)
		using Shader::Shader;

		void SetViewport(int width, int height)
		{
			glm::mat4 proj;
			proj[0][0] = 2.0f / (float) width;
			proj[1][1] = 2.0f / (float) - height;
			proj[2][2] = -1.0f;
			proj[3][0] = -1.0f;
			proj[3][1] = 1.0f;
			proj[3][3] = 1.0f;
			Set("projMat", proj);
		}
	};

	class BasicOrthoFS : public Shader
	{
		SHADER_TYPE(BasicOrthoFS)
		using Shader::Shader;
	};

	class CopyStencilFS : public Shader
	{
		SHADER_TYPE(CopyStencilFS)
		using Shader::Shader;
	};
}