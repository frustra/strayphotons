#include "Lighting.hh"
#include "core/Logging.hh"
#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Util.hh"

namespace sp
{
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

	class LumiHistogramCS : public Shader
	{
		SHADER_TYPE(LumiHistogramCS)

		LumiHistogramCS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
		}
	};

	IMPLEMENT_SHADER_TYPE(LumiHistogramCS, "lumi_histogram.comp", Compute);

	void HDRTest::Process(const PostProcessingContext *context)
	{
		const int wgsize = 8;
		auto r = context->renderer;
		auto dest = outputs[0].AllocateTarget(context)->GetTexture();

		r->ShaderControl->BindPipeline<LumiHistogramCS>(r->GlobalShaders);

		dest.BindImage(0, GL_WRITE_ONLY);

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glDispatchCompute((dest.width + wgsize - 1) / wgsize, (dest.height + wgsize - 1) / wgsize, 1);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
	}
}
