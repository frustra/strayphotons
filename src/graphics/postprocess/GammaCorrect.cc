#include "SMAA.hh"
#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Util.hh"
#include "GammaCorrect.hh"

namespace sp
{
	class GammaCorrectFS : public Shader
	{
		SHADER_TYPE(GammaCorrectFS)
		using Shader::Shader;
	};

	IMPLEMENT_SHADER_TYPE(GammaCorrectFS, "gamma_correct.frag", Fragment);

	void GammaCorrect::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto dest = outputs[0].AllocateTarget(context);

		r->SetRenderTarget(dest, nullptr);
		r->ShaderControl->BindPipeline<BasicPostVS, GammaCorrectFS>(r->GlobalShaders);

		DrawScreenCover();
	}
}