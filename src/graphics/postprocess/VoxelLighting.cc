#include "VoxelLighting.hh"
#include "core/Logging.hh"
#include "core/CVar.hh"
#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Util.hh"

namespace sp
{
	static CVar<int> CVarVoxelLightingDebug("r.VoxelLightingDebug", 0, "Show unprocessed Voxel lighting (1: combined, 2: diffuse, 3: specular, 4: AO)");

	class VoxelLightingFS : public Shader
	{
		SHADER_TYPE(VoxelLightingFS)

		VoxelLightingFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
			Bind(projMat, "projMat");
			Bind(invProjMat, "invProjMat");
			Bind(viewMat, "viewMat");
			Bind(invViewMat, "invViewMat");
			Bind(invViewRotMat, "invViewRotMat");
			Bind(debug, "debug");
		}

		void SetViewParams(const ecs::View &view)
		{
			Set(projMat, view.projMat);
			Set(invProjMat, view.invProjMat);
			Set(viewMat, view.viewMat);
			Set(invViewMat, view.invViewMat);
			Set(invViewRotMat, glm::transpose(glm::mat3(view.viewMat)));
		}

		void SetDebug(int newDebug)
		{
			Set(debug, newDebug);
		}

	private:
		Uniform projMat, invProjMat;
		Uniform viewMat, invViewMat, invViewRotMat;
		Uniform debug;
	};

	IMPLEMENT_SHADER_TYPE(VoxelLightingFS, "voxel_lighting.frag", Fragment);

	void VoxelLighting::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto dest = outputs[0].AllocateTarget(context)->GetTexture();

		r->GlobalShaders->Get<VoxelLightingFS>()->SetViewParams(context->view);
		r->GlobalShaders->Get<VoxelLightingFS>()->SetDebug(CVarVoxelLightingDebug.Get());

		r->SetRenderTarget(&dest, nullptr);
		r->ShaderControl->BindPipeline<BasicPostVS, VoxelLightingFS>(r->GlobalShaders);

		DrawScreenCover();
	}
}
