#include "Lighting.hh"
#include "core/Logging.hh"
#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Util.hh"
#include "ecs/components/View.hh"

namespace sp
{
	class DeferredLightingFS : public Shader
	{
		SHADER_TYPE(DeferredLightingFS)

		DeferredLightingFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
			Bind(viewMat, "viewMat");
			Bind(invViewMat, "invViewMat");
			Bind(invProjMat, "invProjMat");
		}

		void SetViewParams(const ECS::View &view)
		{
			Set(viewMat, view.viewMat);
			Set(invViewMat, view.invViewMat);
			Set(invProjMat, view.invProjMat);
		}

	private:
		Uniform viewMat, invViewMat, invProjMat;
	};

	IMPLEMENT_SHADER_TYPE(DeferredLightingFS, "deferred_lighting.frag", Fragment);

	void DeferredLighting::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto dest = outputs[0].AllocateTarget(context)->GetTexture();

		r->GlobalShaders->Get<DeferredLightingFS>()->SetViewParams(context->view);

		r->SetRenderTarget(&dest, nullptr);
		r->ShaderControl->BindPipeline<BasicPostVS, DeferredLightingFS>(r->GlobalShaders);

		DrawScreenCover();
	}

	class TonemapFS : public Shader
	{
		SHADER_TYPE(TonemapFS)

		TonemapFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
		}
	};

	IMPLEMENT_SHADER_TYPE(TonemapFS, "tonemap.frag", Fragment);

	void Tonemap::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto dest = outputs[0].AllocateTarget(context)->GetTexture();

		r->SetRenderTarget(&dest, nullptr);
		r->ShaderControl->BindPipeline<BasicPostVS, TonemapFS>(r->GlobalShaders);

		DrawScreenCover();
	}
}
