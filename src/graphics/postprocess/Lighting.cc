#include "Lighting.hh"
#include "core/Logging.hh"
#include "core/CVar.hh"
#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Util.hh"
#include "graphics/Buffer.hh"
#include "graphics/GPUTypes.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/Transform.hh"

namespace sp
{
	static CVar<int> CVarVoxelLightingMode("r.VoxelLighting", 1, "Voxel lighting mode (0: direct only, 1: full, 2: indirect only, 3: diffuse only, 4: specular only)");
	static CVar<int> CVarVoxelDiffuseDownsample("r.VoxelDiffuseDownsample", 1, "N times downsampled rendering of indirect diffuse lighting");
	static CVar<bool> CVarDrawHistogram("r.Histogram", false, "Draw HDR luminosity histogram");
	static CVar<float> CVarExposure("r.Exposure", 0.0, "Fixed exposure value in linear units (0: auto)");
	static CVar<float> CVarExposureComp("r.ExposureComp", 1, "Exposure bias in EV units (logarithmic) for eye adaptation");
	static CVar<float> CVarEyeAdaptationLow("r.EyeAdaptationLow", 65, "Percent of darkest pixels to ignore in eye adaptation");
	static CVar<float> CVarEyeAdaptationHigh("r.EyeAdaptationHigh", 92, "Percent of brightest pixels to ignore in eye adaptation");
	static CVar<float> CVarEyeAdaptationMinLuminance("r.EyeAdaptationMinLuminance", 0.01, "Minimum target luminance for eye adaptation");
	static CVar<float> CVarEyeAdaptationMaxLuminance("r.EyeAdaptationMaxLuminance", 10000, "Maximum target luminance for eye adaptation");
	static CVar<float> CVarEyeAdaptationUpRate("r.EyeAdaptationUpRate", 0.1, "Rate at which eye adapts to brighter scenes");
	static CVar<float> CVarEyeAdaptationDownRate("r.EyeAdaptationDownRate", 0.01, "Rate at which eye adapts to darker scenes");
	static CVar<float> CVarEyeAdaptationKeyComp("r.EyeAdaptationKeyComp", 1.0, "Amount of key compensation for eye adaptation (0-1)");

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
		auto &dest = outputs[0].AllocateTarget(context)->GetTexture();

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

		double ComputeScaledLuminance()
		{
			if (!readBackBuf)
			{
				return 0.0f;
			}

			uint32 *buf = (uint32 *) readBackBuf.Map(GL_READ_ONLY);
			if (!buf)
			{
				Errorf("Missed readback of luminosity histogram");
				return 0.0f;
			}

			uint64 sum = 0;

			for (int i = 0; i < Bins; i++)
				sum += buf[i];

			double discardLower = sum * CVarEyeAdaptationLow.Get() / 100.0;
			double discardUpper = sum * CVarEyeAdaptationHigh.Get() / 100.0;
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

				double luminance = LuminanceFromBin(i / (double) (Bins - 1));
				accum += luminance * weight;
				totalWeight += weight;
			}

			readBackBuf.Unmap();
			return accum / std::max(1e-5, totalWeight);
		}

		double LuminanceFromBin(double bin)
		{
			double lumMin = -8, lumMax = 4;
			return std::exp2(bin * (lumMax - lumMin) + lumMin);
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
		Buffer readBackBuf;
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
		auto &histTex = shader->GetTarget(r);

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
			auto &dest = outputs[0].AllocateTarget(context)->GetTexture();
			r->SetRenderTarget(&dest, nullptr);
			r->ShaderControl->BindPipeline<BasicPostVS, RenderHistogramFS>(r->GlobalShaders);
			DrawScreenCover();
		}
		else
		{
			SetOutputTarget(0, GetInput(0)->GetOutput()->TargetRef);
		}
	}

	class VoxelLightingFS : public Shader
	{
		SHADER_TYPE(VoxelLightingFS)

		VoxelLightingFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
			Bind(lightCount, "lightCount");
			Bind(mirrorCount, "mirrorCount");
			BindBuffer(lightData, 0);
			BindBuffer(mirrorData, 1);

			Bind(exposure, "exposure");

			Bind(invProjMat, "invProjMat");
			Bind(invViewMat, "invViewMat");
			Bind(mode, "mode");
			Bind(ssaoEnabled, "ssaoEnabled");

			Bind(voxelSize, "voxelSize");
			Bind(voxelGridCenter, "voxelGridCenter");
			Bind(diffuseDownsample, "diffuseDownsample");
		}

		void SetLightData(int count, GLLightData *data)
		{
			Set(lightCount, count);
			BufferData(lightData, sizeof(GLLightData) * count, data);
		}

		void SetMirrorData(int count, GLMirrorData *data)
		{
			Set(mirrorCount, count);
			BufferData(mirrorData, sizeof(GLMirrorData) * count, data);
		}

		void SetExposure(float newExposure)
		{
			Set(exposure, newExposure);
		}

		void SetViewParams(const ecs::View &view)
		{
			Set(invProjMat, view.invProjMat);
			Set(invViewMat, view.invViewMat);
		}

		void SetMode(int newMode, int ssaoMode)
		{
			Set(mode, newMode);
			Set(ssaoEnabled, ssaoMode);
		}

		void SetVoxelInfo(ecs::VoxelInfo &voxelInfo, int diffDownsample)
		{
			Set(voxelSize, voxelInfo.voxelSize);
			Set(voxelGridCenter, voxelInfo.voxelGridCenter);
			Set(diffuseDownsample, (float) diffDownsample);
		}

	private:
		Uniform lightCount, mirrorCount;
		UniformBuffer lightData, mirrorData;
		Uniform exposure, invViewMat, invProjMat, mode, ssaoEnabled;
		Uniform voxelSize, voxelGridCenter, diffuseDownsample;
	};

	IMPLEMENT_SHADER_TYPE(VoxelLightingFS, "voxel_lighting.frag", Fragment);

	class VoxelLightingDiffuseFS : public Shader
	{
		SHADER_TYPE(VoxelLightingDiffuseFS)

		VoxelLightingDiffuseFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
			Bind(exposure, "exposure");
			Bind(invViewMat, "invViewMat");

			Bind(voxelSize, "voxelSize");
			Bind(voxelGridCenter, "voxelGridCenter");
			Bind(diffuseDownsample, "diffuseDownsample");
		}

		void SetExposure(float newExposure)
		{
			Set(exposure, newExposure);
		}

		void SetViewParams(const ecs::View &view)
		{
			Set(invViewMat, view.invViewMat);
		}

		void SetVoxelInfo(ecs::VoxelInfo &voxelInfo, int diffDownsample)
		{
			Set(voxelSize, voxelInfo.voxelSize);
			Set(voxelGridCenter, voxelInfo.voxelGridCenter);
			Set(diffuseDownsample, (float) diffDownsample);
		}

	private:
		Uniform exposure, invViewMat;
		Uniform voxelSize, voxelGridCenter, diffuseDownsample;
	};

	IMPLEMENT_SHADER_TYPE(VoxelLightingDiffuseFS, "voxel_lighting_diffuse.frag", Fragment);

	void VoxelLighting::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto &dest = outputs[0].AllocateTarget(context)->GetTexture();

		int diffuseDownsample = CVarVoxelDiffuseDownsample.Get();
		if (diffuseDownsample < 1) diffuseDownsample = 1;

		mirrorVisData.Bind(GL_SHADER_STORAGE_BUFFER, 0);
		mirrorSceneData.Bind(GL_SHADER_STORAGE_BUFFER, 1);

		GLLightData lightData[MAX_LIGHTS];
		GLMirrorData mirrorData[MAX_MIRRORS];
		int lightCount = FillLightData(&lightData[0], context->game->entityManager);
		int mirrorCount = FillMirrorData(&mirrorData[0], context->game->entityManager);

		auto shader = r->GlobalShaders->Get<VoxelLightingFS>();
		shader->SetLightData(lightCount, &lightData[0]);
		shader->SetMirrorData(mirrorCount, &mirrorData[0]);
		shader->SetViewParams(context->view);
		shader->SetMode(CVarVoxelLightingMode.Get(), ssaoEnabled);
		shader->SetVoxelInfo(voxelData.info, diffuseDownsample);
		shader->SetExposure(r->Exposure);

		r->SetRenderTarget(&dest, nullptr);
		r->ShaderControl->BindPipeline<BasicPostVS, VoxelLightingFS>(r->GlobalShaders);

		DrawScreenCover();
	}

	VoxelLightingDiffuse::VoxelLightingDiffuse(VoxelData voxelData) : voxelData(voxelData)
	{
		downsample = CVarVoxelDiffuseDownsample.Get();
		if (downsample < 1) downsample = 1;
	}

	void VoxelLightingDiffuse::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto &dest = outputs[0].AllocateTarget(context)->GetTexture();
		auto shader = r->GlobalShaders->Get<VoxelLightingDiffuseFS>();
		auto lumishader = r->GlobalShaders->Get<LumiHistogramCS>();

		shader->SetViewParams(context->view);
		shader->SetVoxelInfo(voxelData.info, downsample);

		if (CVarExposure.Get() > 0.0f)
		{
			r->Exposure = CVarExposure.Get();
		}
		else
		{
			double luminance = lumishader->ComputeScaledLuminance();
			if (luminance > 0)
			{
				luminance = std::min(luminance, (double) CVarEyeAdaptationMaxLuminance.Get());
				luminance = std::max(luminance, (double) CVarEyeAdaptationMinLuminance.Get());
				luminance /= r->Exposure;

				double autoKeyComp = 1.03 - 2.0 / (std::log10(luminance * 1000.0 + 1.0) + 2.0);
				autoKeyComp = CVarEyeAdaptationKeyComp.Get() * autoKeyComp + 1.0 - CVarEyeAdaptationKeyComp.Get();

				double ev100 = std::log2(luminance * 100.0 / 12.5) - CVarExposureComp.Get();
				double newExposure = autoKeyComp / (1.2 * std::pow(2.0, ev100));

				float alpha = newExposure < r->Exposure ? CVarEyeAdaptationUpRate.Get() : CVarEyeAdaptationDownRate.Get();
				alpha = std::max(std::min(alpha, 0.9999f), 0.0001f);
				r->Exposure = r->Exposure * (1.0f - alpha) + newExposure * alpha;
			}
		}

		r->Exposure = std::max(r->Exposure, 1e-5f);
		shader->SetExposure(r->Exposure);

		glViewport(0, 0, outputs[0].TargetDesc.extent[0], outputs[0].TargetDesc.extent[1]);
		r->SetRenderTarget(&dest, nullptr);
		r->ShaderControl->BindPipeline<BasicPostVS, VoxelLightingDiffuseFS>(r->GlobalShaders);

		DrawScreenCover();

		auto view = context->view;
		glViewport(0, 0, view.extents.x, view.extents.y);
	}
}
