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
			Bind(invProj, "invProjMat");
		}

		void SetParameters(int newMode, const ECS::View &view)
		{
			Set(mode, newMode);
			Set(invProj, view.invProjMat);
		}

	private:
		Uniform mode, invProj;
	};

	IMPLEMENT_SHADER_TYPE(ViewGBufferFS, "view_gbuffer.frag", Fragment);

	void ViewGBuffer::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto dest = outputs[0].AllocateTarget(context)->GetTexture();

		r->GlobalShaders->Get<ViewGBufferFS>()->SetParameters(mode, context->view);

		r->SetRenderTarget(&dest, nullptr);
		r->ShaderControl->BindPipeline<BasicPostVS, ViewGBufferFS>(r->GlobalShaders);

		DrawScreenCover();
	}
}