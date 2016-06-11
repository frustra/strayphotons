#include "Lighting.hh"
#include "core/Logging.hh"
#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Util.hh"

namespace sp
{
	class DeferredLightingFS : public Shader
	{
		SHADER_TYPE(DeferredLightingFS)

		DeferredLightingFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
			Bind(view, "viewMat");
			Bind(invView, "invViewMat");
			Bind(invProj, "invProjMat");
		}

		void SetParameters(glm::mat4 newProj, glm::mat4 newView)
		{
			Set(view, newView);
			Set(invView, glm::inverse(newView));
			Set(invProj, glm::inverse(newProj));
		}

	private:
		Uniform view, invView, invProj;
	};

	IMPLEMENT_SHADER_TYPE(DeferredLightingFS, "deferred_lighting.frag", Fragment);

	void DeferredLighting::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto dest = outputs[0].AllocateTarget(context)->GetTexture();

		r->GlobalShaders->Get<DeferredLightingFS>()->SetParameters(r->Projection, r->View);

		r->SetRenderTarget(&dest, nullptr);
		r->ShaderControl->BindPipeline<BasicPostVS, DeferredLightingFS>(r->GlobalShaders);

		DrawScreenCover();
	}
}
