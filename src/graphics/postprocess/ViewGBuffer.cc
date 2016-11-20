#include "ViewGBuffer.hh"
#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Util.hh"

namespace sp
{
	class ViewGBufferFS : public Shader
	{
		SHADER_TYPE(ViewGBufferFS)

		ViewGBufferFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
			Bind(mode, "mode");
			Bind(level, "mipLevel");
			Bind(invProj, "invProjMat");
			Bind(invView, "invViewMat");

			Bind(voxelSize, "voxelSize");
			Bind(voxelGridCenter, "voxelGridCenter");
		}

		void SetParameters(int newMode, int newLevel, const ecs::View &view)
		{
			Set(mode, newMode);
			Set(level, newLevel);
			Set(invProj, view.invProjMat);
			Set(invView, view.invViewMat);
		}

		void SetVoxelInfo(ecs::VoxelInfo &voxelInfo)
		{
			Set(voxelSize, voxelInfo.voxelSize);
			Set(voxelGridCenter, voxelInfo.voxelGridCenter);
		}

	private:
		Uniform mode, level, invProj, invView;
		Uniform voxelSize, voxelGridCenter;
	};

	IMPLEMENT_SHADER_TYPE(ViewGBufferFS, "view_gbuffer.frag", Fragment);

	void ViewGBuffer::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto dest = outputs[0].AllocateTarget(context)->GetTexture();

		r->GlobalShaders->Get<ViewGBufferFS>()->SetParameters(mode, level, context->view);
		r->GlobalShaders->Get<ViewGBufferFS>()->SetVoxelInfo(context->renderer->voxelInfo);

		r->SetRenderTarget(&dest, nullptr);
		r->ShaderControl->BindPipeline<BasicPostVS, ViewGBufferFS>(r->GlobalShaders);

		DrawScreenCover();
	}
}