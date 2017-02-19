#pragma once
#include "graphics/Shader.hh"
#include "graphics/ShaderManager.hh"
#include "ecs/components/View.hh"
#include "ecs/components/Light.hh"
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

		static const int maxLights = 16;

		VoxelRasterFS(shared_ptr<ShaderCompileOutput> compileOutput);

		void SetLights(ecs::EntityManager &manager, ecs::EntityManager::EntityCollection &lightCollection);
		void SetVoxelInfo(ecs::VoxelInfo &voxelInfo);

	private:
		Uniform lightCount, lightPosition, lightTint, lightDirection, lightSpotAngleCos;
		Uniform lightProj, lightInvProj, lightView, lightClip, lightMapOffset, lightIntensity, lightIlluminance;
		Uniform exposure, targetSize, viewMat, invViewMat, invProjMat;
		Uniform voxelSize, voxelGridCenter;
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

	private:
		Uniform level;
	};

	class VoxelClearCS : public Shader
	{
		SHADER_TYPE(VoxelMipmapCS)

		VoxelClearCS(shared_ptr<ShaderCompileOutput> compileOutput);

		void SetLevel(int newLevel);

	private:
		Uniform level;
	};
}
