#include "SSAO.hh"

#include "core/CVar.hh"
#include "core/Logging.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/Util.hh"

#include <random>

namespace sp {
	static CVar<bool> CVarSSAODebug("r.SSAODebug", false, "Show unprocessed SSAO output");

	class SSAOPass0FS : public Shader {
		SHADER_TYPE(SSAOPass0FS)

		SSAOPass0FS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput) {
			GenerateKernel();
		}

		void GenerateKernel() {
			const int samples = 24;
			glm::vec3 offsets[samples];

			std::mt19937 rng;
			std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

			for (int i = 0; i < 24; i++) {
				float distance = float(i) / float(samples);
				distance = (distance * distance) * 0.9f + 0.1f;

				offsets[i].x = dist(rng);
				offsets[i].y = dist(rng);
				offsets[i].z = dist(rng) / 2.0f + 0.5f;

				offsets[i] = normalize(offsets[i]) * distance;
			}

			Set("kernel", offsets, samples);
		}

		void SetViewParams(const ecs::View &view) {
			Set("projMat", view.projMat);
			Set("invProjMat", view.invProjMat);
		}
	};

	IMPLEMENT_SHADER_TYPE(SSAOPass0FS, "ssao_pass0.frag", Fragment);

	class SSAOBlurFS : public Shader {
		SHADER_TYPE(SSAOBlurFS)
		using Shader::Shader;

		void SetParameters(glm::vec2 pattern, const ecs::View &view) {
			Set("samplePattern", pattern);
			Set("invProjMat", view.invProjMat);
		}
	};

	IMPLEMENT_SHADER_TYPE(SSAOBlurFS, "ssao_blur.frag", Fragment);

	struct SSAONoiseTexture {
		Texture tex;

		SSAONoiseTexture(int kernelWidth) {
			int samples = kernelWidth * kernelWidth;
			vector<glm::vec3> noise;

			std::mt19937 rng;
			std::uniform_real_distribution<float> dist(0.0f, 1.0f);

			for (int i = 0; i < samples; i++) { noise.push_back(normalize(glm::vec3{dist(rng), dist(rng), 0.0f})); }

			tex.Create()
				.Filter(GL_NEAREST, GL_NEAREST)
				.Wrap(GL_REPEAT, GL_REPEAT)
				.Size(kernelWidth, kernelWidth)
				.Storage(PF_RGB8F)
				.Image2D(noise.data());
		}
	};

	void SSAOPass0::Process(const PostProcessingContext *context) {
		static SSAONoiseTexture noiseTex(5);

		auto r = context->renderer;
		auto dest = outputs[0].AllocateTarget(context);

		r->GlobalShaders->Get<SSAOPass0FS>()->SetViewParams(context->view);

		r->SetRenderTarget(dest, nullptr);
		r->ShaderControl->BindPipeline<BasicPostVS, SSAOPass0FS>();

		noiseTex.tex.Bind(3);

		auto desc = dest->GetDesc();
		glViewport(0, 0, desc.extent.x, desc.extent.y);
		DrawScreenCover();
	}

	void SSAOBlur::Process(const PostProcessingContext *context) {
		auto r = context->renderer;
		auto dest = outputs[0].AllocateTarget(context);

		if (!CVarSSAODebug.Get()) {
			auto extent = GetInput(0)->GetOutput()->TargetDesc.extent;

			glm::vec2 samplePattern;

			if (horizontal)
				samplePattern.x = 1.0f / (float)extent.x;
			else
				samplePattern.y = 1.0f / (float)extent.y;

			r->GlobalShaders->Get<SSAOBlurFS>()->SetParameters(samplePattern, context->view);

			r->ShaderControl->BindPipeline<BasicPostVS, SSAOBlurFS>();
		} else {
			r->ShaderControl->BindPipeline<BasicPostVS, ScreenCoverFS>();
		}

		r->SetRenderTarget(dest, nullptr);
		auto desc = dest->GetDesc();
		glViewport(0, 0, desc.extent.x, desc.extent.y);
		DrawScreenCover();
	}
} // namespace sp
