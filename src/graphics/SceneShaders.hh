#pragma once
#include "graphics/Shader.hh"
#include "graphics/ShaderManager.hh"
#include "ecs/components/View.hh"

namespace sp
{
	class SceneShader : public Shader
	{
	public:
		void SetParams(const ecs::View &view, glm::mat4 modelMat);

	protected:
		SceneShader(shared_ptr<ShaderCompileOutput> compileOutput);

	private:
		Uniform modelMat, viewMat, projMat;
	};

	class SceneVS : public SceneShader
	{
		SHADER_TYPE(SceneVS)
		using SceneShader::SceneShader;
	};

	class SceneGS : public Shader
	{
		SHADER_TYPE(SceneGS)
		using Shader::Shader;
	};

	class SceneFS : public Shader
	{
		SHADER_TYPE(SceneFS)
		using Shader::Shader;
	};

	class ShadowMapVS : public SceneShader
	{
		SHADER_TYPE(ShadowMapVS)
		using SceneShader::SceneShader;
	};

	class ShadowMapFS : public Shader
	{
		SHADER_TYPE(ShadowMapFS)

		ShadowMapFS(shared_ptr<ShaderCompileOutput> compileOutput);

		void SetClip(glm::vec2 newClip);

	private:
		Uniform clip;
	};

	class VoxelVS : public SceneShader
	{
		SHADER_TYPE(VoxelVS)
		using SceneShader::SceneShader;
	};

	class VoxelGS : public Shader
	{
		SHADER_TYPE(VoxelGS)
		using Shader::Shader;
	};

	class VoxelFS : public Shader
	{
		SHADER_TYPE(VoxelFS)
		using Shader::Shader;
	};

	class VoxelClearFS : public Shader
	{
		SHADER_TYPE(VoxelClearFS)

		VoxelClearFS(shared_ptr<ShaderCompileOutput> compileOutput);

		void SetDepth(int newDepth);

	private:
		Uniform depth;
	};
}
