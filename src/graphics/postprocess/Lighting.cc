#include "Lighting.hh"
#include "core/Logging.hh"
#include "core/CVar.hh"
#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Util.hh"
#include "graphics/Buffer.hh"

namespace sp
{
	static CVar<bool> CVarDrawHistogram("r.Histogram", false, "Draw HDR luminosity histogram");

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

		const int Bins = 64;

		LumiHistogramCS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
		}

		Texture &GetTarget(Renderer *r)
		{
			if (!target)
			{
				target = r->RTPool->Get(RenderTargetDesc(PF_R32UI, { Bins, 1 }));
			}
			return target->GetTexture();
		}

		float ComputeExposure()
		{
			if (!readBackBuf)
			{
				return 0.0f;
			}

			uint32 *buf = (uint32 *) readBackBuf.Map(GL_READ_ONLY);
			if (!buf)
			{
				Logf("Missed readback of luminosity histogram");
				return 0.0f;
			}

			uint64 sum = 0;

			for (int i = 0; i < Bins; i++)
				sum += buf[i];

			double discardLower = sum * 0.5, discardUpper = sum * 0.9;
			double accum = 0.0, totalWeight = 0.0;

			for (int i = 0; i < Bins; i++)
			{
				double weight = buf[i];
				double m = std::min(weight, discardLower);

				weight -= m; // discard samples
				discardLower -= m;
				discardUpper -= m;

				weight = std::min(weight, discardUpper);
				discardUpper -= weight;

				double luminance = LuminanceFromBin(i / (double) Bins);
				accum += luminance * weight;
				totalWeight += weight;
			}

			accum /= std::max(1e-5, totalWeight);
			readBackBuf.Unmap();

			float exposure = 0.0f;
			// TODO
			return exposure;
		}

		double LuminanceFromBin(double bin)
		{
			return std::exp2(bin);
		}

		void StartReadback()
		{
			if (!readBackBuf)
			{
				readBackBuf.Create().Data(sizeof(uint32) * Bins, nullptr, GL_STREAM_READ);
			}

			readBackBuf.Bind(GL_PIXEL_PACK_BUFFER);
			glGetTextureImage(target->GetTexture().handle, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, sizeof(uint32) * Bins, 0);
		}

	private:
		shared_ptr<RenderTarget> target;
		sp::Buffer readBackBuf;
	};

	IMPLEMENT_SHADER_TYPE(LumiHistogramCS, "lumi_histogram.comp", Compute);

	class RenderHistogramFS : public Shader
	{
		SHADER_TYPE(RenderHistogramFS)

		RenderHistogramFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
		}
	};

	IMPLEMENT_SHADER_TYPE(RenderHistogramFS, "render_histogram.frag", Fragment);

	void LumiHistogram::Process(const PostProcessingContext *context)
	{
		const int wgsize = 16;
		const int downsample = 2; // Calculate histograms with N times fewer workgroups.

		auto r = context->renderer;
		auto shader = r->GlobalShaders->Get<LumiHistogramCS>();
		auto histTex = shader->GetTarget(r);

		float lastExposure = shader->ComputeExposure();
		if (lastExposure != 0.0f) Logf("%f", lastExposure);

		r->SetRenderTarget(&histTex, nullptr);
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);

		r->ShaderControl->BindPipeline<LumiHistogramCS>(r->GlobalShaders);
		histTex.BindImage(0, GL_READ_WRITE);

		auto extents = GetInput(0)->GetOutput()->TargetDesc.extent / downsample;
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glDispatchCompute((extents.x + wgsize - 1) / wgsize, (extents.y + wgsize - 1) / wgsize, 1);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

		shader->StartReadback();

		if (CVarDrawHistogram.Get())
		{
			auto dest = outputs[0].AllocateTarget(context)->GetTexture();
			r->SetRenderTarget(&dest, nullptr);
			r->ShaderControl->BindPipeline<BasicPostVS, RenderHistogramFS>(r->GlobalShaders);
			DrawScreenCover();
		}
		else
		{
			SetOutputTarget(0, GetInput(0)->GetOutput()->TargetRef);
		}
	}
}
