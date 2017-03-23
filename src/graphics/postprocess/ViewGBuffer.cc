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
			BindBuffer(voxelInfo, 0);

			Bind(mode, "mode");
			Bind(source, "source");
			Bind(level, "mipLevel");
			Bind(invProj, "invProjMat");
			Bind(invView, "invViewMat");
		}

		void SetParameters(int newMode, int newSource, int newLevel, const ecs::View &view)
		{
			Set(mode, newMode);
			Set(source, newSource);
			Set(level, newLevel);
			Set(invProj, view.invProjMat);
			Set(invView, view.invViewMat);
		}

		void SetVoxelInfo(GLVoxelInfo *data)
		{
			BufferData(voxelInfo, sizeof(GLVoxelInfo), data);
		}

	private:
		UniformBuffer voxelInfo;
		Uniform mode, source, level, invProj, invView;
	};

	IMPLEMENT_SHADER_TYPE(ViewGBufferFS, "view_gbuffer.frag", Fragment);

	void ViewGBuffer::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto &dest = outputs[0].AllocateTarget(context)->GetTexture();

		GLVoxelInfo voxelInfo;
		FillVoxelInfo(&voxelInfo, voxelData.info);

		r->GlobalShaders->Get<ViewGBufferFS>()->SetParameters(mode, source, level, context->view);
		r->GlobalShaders->Get<ViewGBufferFS>()->SetVoxelInfo(&voxelInfo);

		r->SetRenderTarget(&dest, nullptr);
		r->ShaderControl->BindPipeline<BasicPostVS, ViewGBufferFS>(r->GlobalShaders);

		DrawScreenCover();
	}
}