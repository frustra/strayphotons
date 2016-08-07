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

	ShadowMapFS::ShadowMapFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		Bind(clip, "clip");
	}

	void ShadowMapFS::SetClip(glm::vec2 newClip)
	{
		Set(clip, newClip);
	}

	VoxelClearFS::VoxelClearFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		Bind(depth, "depth");
	}

	void VoxelClearFS::SetDepth(int newDepth)
	{
		Set(depth, newDepth);
	}

	IMPLEMENT_SHADER_TYPE(SceneVS, "scene.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(SceneGS, "scene.geom", Geometry);
	IMPLEMENT_SHADER_TYPE(SceneFS, "scene.frag", Fragment);

	IMPLEMENT_SHADER_TYPE(ShadowMapVS, "shadow_map.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(ShadowMapFS, "shadow_map.frag", Fragment);

	IMPLEMENT_SHADER_TYPE(VoxelVS, "voxel.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(VoxelGS, "voxel.geom", Geometry);
	IMPLEMENT_SHADER_TYPE(VoxelFS, "voxel.frag", Fragment);
	IMPLEMENT_SHADER_TYPE(VoxelClearFS, "voxel_clear.frag", Fragment);
}
