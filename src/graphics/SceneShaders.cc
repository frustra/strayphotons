#include "SceneShaders.hh"

namespace sp
{
	SceneShader::SceneShader(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		Bind(modelMat, "model");
		Bind(viewMat, "view");
		Bind(projMat, "projection");
	}

	void SceneShader::SetParams(const ecs::View &view, glm::mat4 modelMat)
	{
		Set(this->modelMat, modelMat);
		Set(viewMat, view.viewMat);
		Set(projMat, view.projMat);
	}

	SceneGS::SceneGS(shared_ptr<ShaderCompileOutput> compileOutput) : SceneShader(compileOutput)
	{
		Bind(renderMirrors, "renderMirrors");
	}

	void SceneGS::SetRenderMirrors(bool v)
	{
		Set(renderMirrors, v);
	}

	SceneFS::SceneFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		Bind(emissive, "emissive");
		Bind(mirrorId, "drawMirrorId");
	}

	void SceneFS::SetEmissive(float scale)
	{
		Set(emissive, scale);
	}

	void SceneFS::SetMirrorId(int newId)
	{
		Set(mirrorId, newId);
	}

	MirrorSceneCS::MirrorSceneCS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		Bind(mirrorCount, "mirrorCount");
		BindBuffer(mirrorData, 0);
	}

	void MirrorSceneCS::SetMirrorData(int count, GLMirrorData *data)
	{
		Set(mirrorCount, count);
		BufferData(mirrorData, sizeof(GLMirrorData) * count, data);
	}

	ShadowMapFS::ShadowMapFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		Bind(clip, "clip");
		Bind(lightId, "drawLightId");
		Bind(mirrorId, "drawMirrorId");
	}

	void ShadowMapFS::SetClip(glm::vec2 newClip)
	{
		Set(clip, newClip);
	}

	void ShadowMapFS::SetLight(int newLightId)
	{
		Set(lightId, newLightId);
	}

	void ShadowMapFS::SetMirrorId(int newId)
	{
		Set(mirrorId, newId);
	}

	MirrorMapCS::MirrorMapCS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		Bind(lightCount, "lightCount");
		Bind(mirrorCount, "mirrorCount");
		BindBuffer(lightData, 0);
		BindBuffer(mirrorData, 1);
	}

	void MirrorMapCS::SetLightData(int count, GLLightData *data)
	{
		Set(lightCount, count);
		BufferData(lightData, sizeof(GLLightData) * count, data);
	}

	void MirrorMapCS::SetMirrorData(int count, GLMirrorData *data)
	{
		Set(mirrorCount, count);
		BufferData(mirrorData, sizeof(GLMirrorData) * count, data);
	}

	MirrorMapFS::MirrorMapFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		Bind(mirrorId, "drawMirrorId");
		Bind(lightCount, "lightCount");
		BindBuffer(lightData, 0);
	}

	void MirrorMapFS::SetLightData(int count, GLLightData *data)
	{
		Set(lightCount, count);
		BufferData(lightData, sizeof(GLLightData) * count, data);
	}

	void MirrorMapFS::SetMirrorId(int newId)
	{
		Set(mirrorId, newId);
	}

	VoxelMipmapCS::VoxelMipmapCS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		BindBuffer(voxelInfo, 0);
		Bind(level, "mipLevel");
	}

	void VoxelMipmapCS::SetLevel(int newLevel)
	{
		Set(level, newLevel);
	}

	void VoxelMipmapCS::SetVoxelInfo(GLVoxelInfo *data)
	{
		BufferData(voxelInfo, sizeof(GLVoxelInfo), data);
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
		BindBuffer(lightData, 0);
		BindBuffer(voxelInfo, 1);

		Bind(viewMat, "viewMat");
		Bind(invViewMat, "invViewMat");
		Bind(invProjMat, "invProjMat");

		Bind(lightAttenuation, "lightAttenuation");
	}

	void VoxelRasterFS::SetLightData(int count, GLLightData *data)
	{
		Set(lightCount, count);
		BufferData(lightData, sizeof(GLLightData) * count, data);
	}

	void VoxelRasterFS::SetVoxelInfo(GLVoxelInfo *data)
	{
		BufferData(voxelInfo, sizeof(GLVoxelInfo), data);
	}

	void VoxelRasterFS::SetLightAttenuation(float newAttenuation)
	{
		Set(lightAttenuation, newAttenuation);
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

	IMPLEMENT_SHADER_TYPE(VoxelRasterVS, "voxel.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(VoxelRasterGS, "voxel.geom", Geometry);
	IMPLEMENT_SHADER_TYPE(VoxelRasterFS, "voxel.frag", Fragment);
	IMPLEMENT_SHADER_TYPE(VoxelConvertCS, "voxel_convert.comp", Compute);
	IMPLEMENT_SHADER_TYPE(VoxelMipmapCS, "voxel_mipmap.comp", Compute);
	IMPLEMENT_SHADER_TYPE(VoxelClearCS, "voxel_clear.comp", Compute);
}
