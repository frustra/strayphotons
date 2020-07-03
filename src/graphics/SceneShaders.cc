#include "SceneShaders.hh"

namespace sp
{
	SceneShader::SceneShader(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		BindBuffer(boneData, 2);
	}

	void SceneShader::SetParams(const ecs::View &view, glm::mat4 modelMat, glm::mat4 primitiveMat)
	{
		Set("model", modelMat);
		Set("primitive", primitiveMat);
		Set("view", view.viewMat);
		Set("projection", view.projMat);
		Set("boneCount", 0);
	}

	void SceneShader::SetBoneData(int count, glm::mat4* bones)
	{
		Set("boneCount", count);
		BufferData(boneData, sizeof(glm::mat4) * count, bones);
	}

	void SceneGS::SetRenderMirrors(bool v)
	{
		Set("renderMirrors", v);
	}

	void SceneFS::SetEmissive(glm::vec3 em)
	{
		Set("emissive", em);
	}

	void SceneFS::SetMirrorId(int newId)
	{
		Set("drawMirrorId", newId);
	}

	MirrorSceneCS::MirrorSceneCS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		BindBuffer(mirrorData, 0);
	}

	void MirrorSceneCS::SetMirrorData(int count, GLMirrorData *data)
	{
		Set("mirrorCount", count);
		BufferData(mirrorData, sizeof(GLMirrorData) * count, data);
	}

	void ShadowMapFS::SetClip(glm::vec2 newClip)
	{
		Set("clip", newClip);
	}

	void ShadowMapFS::SetLight(int newLightId)
	{
		Set("drawLightId", newLightId);
	}

	void ShadowMapFS::SetMirrorId(int newId)
	{
		Set("drawMirrorId", newId);
	}

	MirrorMapCS::MirrorMapCS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		BindBuffer(lightData, 0);
		BindBuffer(mirrorData, 1);
	}

	void MirrorMapCS::SetLightData(int count, GLLightData *data)
	{
		Set("lightCount", count);
		BufferData(lightData, sizeof(GLLightData) * count, data);
	}

	void MirrorMapCS::SetMirrorData(int count, GLMirrorData *data)
	{
		Set("mirrorCount", count);
		BufferData(mirrorData, sizeof(GLMirrorData) * count, data);
	}

	MirrorMapFS::MirrorMapFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		BindBuffer(lightData, 0);
	}

	void MirrorMapFS::SetLightData(int count, GLLightData *data)
	{
		Set("lightCount", count);
		BufferData(lightData, sizeof(GLLightData) * count, data);
	}

	void MirrorMapFS::SetMirrorId(int newId)
	{
		Set("drawMirrorId", newId);
	}

	void VoxelMergeCS::SetLevel(int newLevel)
	{
		Set("mipLevel", newLevel);
	}

	VoxelMipmapCS::VoxelMipmapCS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		BindBuffer(voxelInfo, 0);
	}

	void VoxelMipmapCS::SetLevel(int newLevel)
	{
		Set("mipLevel", newLevel);
	}

	void VoxelMipmapCS::SetVoxelInfo(GLVoxelInfo *data)
	{
		BufferData(voxelInfo, sizeof(GLVoxelInfo), data);
	}

	void VoxelClearCS::SetLevel(int newLevel)
	{
		Set("mipLevel", newLevel);
	}

	VoxelFillFS::VoxelFillFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		BindBuffer(lightData, 0);
		BindBuffer(voxelInfo, 1);
	}

	void VoxelFillFS::SetLightData(int count, GLLightData *data)
	{
		Set("lightCount", count);
		BufferData(lightData, sizeof(GLLightData) * count, data);
	}

	void VoxelFillFS::SetVoxelInfo(GLVoxelInfo *data)
	{
		BufferData(voxelInfo, sizeof(GLVoxelInfo), data);
	}

	void VoxelFillFS::SetLightAttenuation(float newAttenuation)
	{
		Set("lightAttenuation", newAttenuation);
	}

	IMPLEMENT_SHADER_TYPE(SceneVS, "scene.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(SceneGS, "scene.geom", Geometry);
	IMPLEMENT_SHADER_TYPE(SceneFS, "scene.frag", Fragment);
	IMPLEMENT_SHADER_TYPE(MirrorSceneCS, "mirror_scene.comp", Compute);
	IMPLEMENT_SHADER_TYPE(SceneDepthClearVS, "scene_depth_clear.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(SceneDepthClearFS, "scene_depth_clear.frag", Fragment);

	IMPLEMENT_SHADER_TYPE(ShadowMapVS, "shadow_map.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(ShadowMapFS, "shadow_map.frag", Fragment);

	IMPLEMENT_SHADER_TYPE(MirrorMapVS, "mirror_shadow_map.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(MirrorMapGS, "mirror_shadow_map.geom", Geometry);
	IMPLEMENT_SHADER_TYPE(MirrorMapFS, "mirror_shadow_map.frag", Fragment);
	IMPLEMENT_SHADER_TYPE(MirrorMapCS, "mirror_shadow_map.comp", Compute);

	IMPLEMENT_SHADER_TYPE(VoxelFillVS, "voxel_fill.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(VoxelFillGS, "voxel_fill.geom", Geometry);
	IMPLEMENT_SHADER_TYPE(VoxelFillFS, "voxel_fill.frag", Fragment);
	IMPLEMENT_SHADER_TYPE(VoxelMergeCS, "voxel_merge.comp", Compute);
	IMPLEMENT_SHADER_TYPE(VoxelMipmapCS, "voxel_mipmap.comp", Compute);
	IMPLEMENT_SHADER_TYPE(VoxelClearCS, "voxel_clear.comp", Compute);
}
