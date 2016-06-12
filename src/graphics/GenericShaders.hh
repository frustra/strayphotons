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

	class BasicOrthoVS : public Shader
	{
		SHADER_TYPE(BasicOrthoVS)

		BasicOrthoVS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
			Bind(projMat, "projMat");
		}

		void SetViewport(float width, float height)
		{
			glm::mat4 proj;
			proj[0][0] = 2.0f / width;
			proj[1][1] = 2.0f / -height;
			proj[2][2] = -1.0f;
			proj[3][0] = -1.0f;
			proj[3][1] = 1.0f;
			proj[3][3] = 1.0f;
			Set(projMat, proj);
		}

	private:
		Uniform projMat;
	};

	class BasicOrthoFS : public Shader
	{
		SHADER_TYPE(BasicOrthoFS)
		using Shader::Shader;
	};
}