#pragma once
#include "graphics/Shader.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GPUTypes.hh"
#include "ecs/components/View.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/LightSensor.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/VoxelInfo.hh"
#include <Ecs.hh>

namespace sp
{
	class SceneShader : public Shader
	{
	public:
		SceneShader(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput) {}
		void SetParams(const ecs::View &view, glm::mat4 modelMat, glm::mat4 primitiveMat);
	};

	class SceneVS : public SceneShader
	{
		SHADER_TYPE(SceneVS)
		using SceneShader::SceneShader;
	};

	class SceneGS : public SceneShader
	{
		SHADER_TYPE(SceneGS)
		using SceneShader::SceneShader;

		void SetRenderMirrors(bool v);
	};

	class SceneFS : public Shader
	{
		SHADER_TYPE(SceneFS)
		using Shader::Shader;

		void SetEmissive(glm::vec3 em);
		void SetMirrorId(int id);
	};

	class MirrorSceneCS : public Shader
	{
		SHADER_TYPE(MirrorSceneCS)

		MirrorSceneCS(shared_ptr<ShaderCompileOutput> compileOutput);
		void SetMirrorData(int count, GLMirrorData *data);

	private:
		UniformBuffer mirrorData;
	};

	class SceneDepthClearVS : public SceneShader
	{
		SHADER_TYPE(SceneDepthClearVS)
		using SceneShader::SceneShader;
	};

	class SceneDepthClearFS : public SceneShader
	{
		SHADER_TYPE(SceneDepthClearFS)
		using SceneShader::SceneShader;
	};

	class ShadowMapVS : public SceneShader
	{
		SHADER_TYPE(ShadowMapVS)
		using SceneShader::SceneShader;
	};

	class ShadowMapFS : public Shader
	{
		SHADER_TYPE(ShadowMapFS)
		using Shader::Shader;

		void SetClip(glm::vec2 newClip);
		void SetLight(int lightId);
		void SetMirrorId(int id);
	};

	class MirrorMapCS : public Shader
	{
		SHADER_TYPE(MirrorMapCS)

		MirrorMapCS(shared_ptr<ShaderCompileOutput> compileOutput);

		void SetLightData(int count, GLLightData *data);
		void SetMirrorData(int count, GLMirrorData *data);

	private:
		UniformBuffer lightData, mirrorData;
	};

	class MirrorMapVS : public SceneShader
	{
		SHADER_TYPE(MirrorMapVS)
		using SceneShader::SceneShader;
	};

	class MirrorMapGS : public Shader
	{
		SHADER_TYPE(MirrorMapGS)
		using Shader::Shader;
	};

	class MirrorMapFS : public Shader
	{
		SHADER_TYPE(MirrorMapFS)

		MirrorMapFS(shared_ptr<ShaderCompileOutput> compileOutput);

		void SetLightData(int count, GLLightData *data);
		void SetMirrorId(int id);

	private:
		UniformBuffer lightData;
	};

	class VoxelFillVS : public SceneShader
	{
		SHADER_TYPE(VoxelFillVS)
		using SceneShader::SceneShader;
	};

	class VoxelFillGS : public Shader
	{
		SHADER_TYPE(VoxelFillGS)
		using Shader::Shader;
	};

	class VoxelFillFS : public Shader
	{
		SHADER_TYPE(VoxelFillFS)

		VoxelFillFS(shared_ptr<ShaderCompileOutput> compileOutput);

		void SetLightData(int count, GLLightData *data);
		void SetVoxelInfo(GLVoxelInfo *data);
		void SetLightAttenuation(float attenuation);

	private:
		UniformBuffer lightData, voxelInfo;
	};

	class VoxelMergeCS : public Shader
	{
		SHADER_TYPE(VoxelMergeCS)
		using Shader::Shader;

		void SetLevel(int newLevel);
	};

	class VoxelMipmapCS : public Shader
	{
		SHADER_TYPE(VoxelMipmapCS)

		VoxelMipmapCS(shared_ptr<ShaderCompileOutput> compileOutput);

		void SetLevel(int newLevel);
		void SetVoxelInfo(GLVoxelInfo *data);

	private:
		UniformBuffer voxelInfo;
	};

	class VoxelClearCS : public Shader
	{
		SHADER_TYPE(VoxelClearCS)
		using Shader::Shader;

		void SetLevel(int newLevel);
	};
}
