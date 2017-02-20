#include "SceneShaders.hh"

namespace sp
{
	SceneShader::SceneShader(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		Bind(modelMat, "model");
		Bind(viewMat, "view");
		Bind(projMat, "projection");

		Bind(mirrorId, "mirrorId");
	}

	void SceneShader::SetParams(const ecs::View &view, glm::mat4 modelMat)
	{
		Set(this->modelMat, modelMat);
		Set(viewMat, view.viewMat);
		Set(projMat, view.projMat);
	}

	void SceneShader::SetMirrorId(int newId)
	{
		Set(mirrorId, newId);
	}

	ShadowMapFS::ShadowMapFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		Bind(clip, "clip");
		Bind(lightId, "lightId");
	}

	void ShadowMapFS::SetClip(glm::vec2 newClip)
	{
		Set(clip, newClip);
	}

	void ShadowMapFS::SetLight(int newLightId)
	{
		Set(lightId, newLightId);
	}

	VoxelMipmapCS::VoxelMipmapCS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		Bind(level, "mipLevel");
	}

	void VoxelMipmapCS::SetLevel(int newLevel)
	{
		Set(level, newLevel);
	}

	VoxelClearCS::VoxelClearCS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		Bind(level, "mipLevel");
	}

	void VoxelClearCS::SetLevel(int newLevel)
	{
		Set(level, newLevel);
	}

	VoxelRasterFS::VoxelRasterFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		Bind(lightCount, "lightCount");
		Bind(lightPosition, "lightPosition");
		Bind(lightTint, "lightTint");
		Bind(lightDirection, "lightDirection");
		Bind(lightSpotAngleCos, "lightSpotAngleCos");
		Bind(lightProj, "lightProj");
		Bind(lightInvProj, "lightInvProj");
		Bind(lightView, "lightView");
		Bind(lightClip, "lightClip");
		Bind(lightMapOffset, "lightMapOffset");
		Bind(lightIntensity, "lightIntensity");
		Bind(lightIlluminance, "lightIlluminance");

		Bind(exposure, "exposure");
		Bind(targetSize, "targetSize");

		Bind(viewMat, "viewMat");
		Bind(invViewMat, "invViewMat");
		Bind(invProjMat, "invProjMat");

		Bind(voxelSize, "voxelSize");
		Bind(voxelGridCenter, "voxelGridCenter");
	}

	void VoxelRasterFS::SetLights(ecs::EntityManager &manager, ecs::EntityManager::EntityCollection &lightCollection)
	{
		glm::vec3 lightPositions[maxLights];
		glm::vec3 lightTints[maxLights];
		glm::vec3 lightDirections[maxLights];
		float lightSpotAnglesCos[maxLights];
		glm::mat4 lightProjs[maxLights];
		glm::mat4 lightInvProjs[maxLights];
		glm::mat4 lightViews[maxLights];
		glm::vec2 lightClips[maxLights];
		glm::vec4 lightMapOffsets[maxLights];
		float lightIntensities[maxLights];
		float lightIlluminances[maxLights];

		int lightNum = 0;
		for (auto entity : lightCollection)
		{
			auto light = entity.Get<ecs::Light>();
			auto view = entity.Get<ecs::View>();
			auto transform = entity.Get<ecs::Transform>();
			lightPositions[lightNum] = transform->GetModelTransform(manager) * glm::vec4(0, 0, 0, 1);
			lightTints[lightNum] = light->tint;
			lightDirections[lightNum] = glm::mat3(transform->GetModelTransform(manager)) * glm::vec3(0, 0, -1);
			lightSpotAnglesCos[lightNum] = cos(light->spotAngle);
			lightProjs[lightNum] = view->projMat;
			lightInvProjs[lightNum] = view->invProjMat;
			lightViews[lightNum] = view->viewMat;
			lightClips[lightNum] = view->clip;
			lightMapOffsets[lightNum] = light->mapOffset;
			lightIntensities[lightNum] = light->intensity;
			lightIlluminances[lightNum] = light->illuminance;
			lightNum++;
		}

		Set(lightCount, lightNum);
		Set(lightPosition, lightPositions, lightNum);
		Set(lightTint, lightTints, lightNum);
		Set(lightDirection, lightDirections, lightNum);
		Set(lightSpotAngleCos, lightSpotAnglesCos, lightNum);
		Set(lightProj, lightProjs, lightNum);
		Set(lightInvProj, lightInvProjs, lightNum);
		Set(lightView, lightViews, lightNum);
		Set(lightClip, lightClips, lightNum);
		Set(lightMapOffset, lightMapOffsets, lightNum);
		Set(lightIntensity, lightIntensities, lightNum);
		Set(lightIlluminance, lightIlluminances, lightNum);
	}

	void VoxelRasterFS::SetVoxelInfo(ecs::VoxelInfo &voxelInfo)
	{
		Set(voxelSize, voxelInfo.voxelSize);
		Set(voxelGridCenter, voxelInfo.voxelGridCenter);
	}

	IMPLEMENT_SHADER_TYPE(SceneVS, "scene.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(SceneGS, "scene.geom", Geometry);
	IMPLEMENT_SHADER_TYPE(SceneFS, "scene.frag", Fragment);

	IMPLEMENT_SHADER_TYPE(ShadowMapVS, "shadow_map.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(ShadowMapFS, "shadow_map.frag", Fragment);

	IMPLEMENT_SHADER_TYPE(VoxelRasterVS, "voxel.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(VoxelRasterGS, "voxel.geom", Geometry);
	IMPLEMENT_SHADER_TYPE(VoxelRasterFS, "voxel.frag", Fragment);
	IMPLEMENT_SHADER_TYPE(VoxelConvertCS, "voxel_convert.comp", Compute);
	IMPLEMENT_SHADER_TYPE(VoxelMipmapCS, "voxel_mipmap.comp", Compute);
	IMPLEMENT_SHADER_TYPE(VoxelClearCS, "voxel_clear.comp", Compute);
}
