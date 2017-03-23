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

	class SceneGS : public SceneShader
	{
		SHADER_TYPE(SceneGS)
		SceneGS(shared_ptr<ShaderCompileOutput> compileOutput);
		void SetRenderMirrors(bool v);

	private:
		Uniform renderMirrors;
	};

	class SceneFS : public Shader
	{
		SHADER_TYPE(SceneFS)
		SceneFS(shared_ptr<ShaderCompileOutput> compileOutput);

		void SetMirrorId(int id);

	private:
		Uniform mirrorId;
	};

	class MirrorSceneCS : public Shader
	{
		SHADER_TYPE(MirrorSceneCS)

		MirrorSceneCS(shared_ptr<ShaderCompileOutput> compileOutput);
		void SetMirrorData(int count, GLMirrorData *data);

	private:
		UniformBuffer mirrorData;
		Uniform mirrorCount;
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

		ShadowMapFS(shared_ptr<ShaderCompileOutput> compileOutput);

		void SetClip(glm::vec2 newClip);
		void SetLight(int lightId);
		void SetMirrorId(int id);

	private:
		Uniform clip, lightId, mirrorId;
	};

	class MirrorMapCS : public Shader
	{
		SHADER_TYPE(MirrorMapCS)

		MirrorMapCS(shared_ptr<ShaderCompileOutput> compileOutput);

		void SetLightData(int count, GLLightData *data);
		void SetMirrorData(int count, GLMirrorData *data);

	private:
		UniformBuffer lightData, mirrorData;
		Uniform lightCount, mirrorCount;
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
		Uniform mirrorId, lightCount;
		UniformBuffer lightData;
	};

	class VoxelRasterVS : public SceneShader
	{
		SHADER_TYPE(VoxelRasterVS)
		using SceneShader::SceneShader;
	};

	class VoxelRasterGS : public Shader
	{
		SHADER_TYPE(VoxelRasterGS)
		using Shader::Shader;
	};

	class VoxelRasterFS : public Shader
	{
		SHADER_TYPE(VoxelRasterFS)

		VoxelRasterFS(shared_ptr<ShaderCompileOutput> compileOutput);

		void SetLightData(int count, GLLightData *data);
		void SetVoxelInfo(GLVoxelInfo *data);
		void SetLightAttenuation(float attenuation);

	private:
		Uniform lightCount;
		UniformBuffer lightData, voxelInfo;
		Uniform viewMat, invViewMat, invProjMat;
		Uniform lightAttenuation;
	};

	class VoxelConvertCS : public Shader
	{
		SHADER_TYPE(VoxelConvertCS)
		using Shader::Shader;
	};

	class VoxelMipmapCS : public Shader
	{
		SHADER_TYPE(VoxelMipmapCS)

		VoxelMipmapCS(shared_ptr<ShaderCompileOutput> compileOutput);

		void SetLevel(int newLevel);
		void SetVoxelInfo(GLVoxelInfo *data);

	private:
		UniformBuffer voxelInfo;
		Uniform level;
	};

	class VoxelClearCS : public Shader
	{
		SHADER_TYPE(VoxelClearCS)

		VoxelClearCS(shared_ptr<ShaderCompileOutput> compileOutput);

		void SetLevel(int newLevel);

	private:
		Uniform level;
	};
}
