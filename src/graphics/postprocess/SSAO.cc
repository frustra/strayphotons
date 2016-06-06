#include "SSAO.hh"
#include "core/Logging.hh"
#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Util.hh"

#include <random>

namespace sp
{
	class SSAOPass0VS : public Shader
	{
		SHADER_TYPE(SSAOPass0VS)
		using Shader::Shader;
	};

	class SSAOPass0FS : public Shader
	{
		SHADER_TYPE(SSAOPass0FS)

		SSAOPass0FS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
			Bind(projection, "projMatrix");
			Bind(kernel, "kernel");

			GenerateKernel();
		}

		void GenerateKernel()
		{
			const int samples = 24;
			glm::vec3 offsets[samples];

			std::mt19937 rng;
			std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

			for (int i = 0; i < 24; i++)
			{
				float distance = float(i) / float(samples);
				distance = (distance * distance) * 0.9 + 0.1;

				offsets[i].x = dist(rng);
				offsets[i].y = dist(rng);
				offsets[i].z = dist(rng) / 2.0f + 0.5f;

				offsets[i] = normalize(offsets[i]) * distance;
			}

			Set(kernel, offsets, samples);
		}

		void SetProjection(glm::mat4 proj)
		{
			Set(projection, proj);
		}

	private:
		Uniform projection, kernel;
	};

	IMPLEMENT_SHADER_TYPE(SSAOPass0VS, "ssao_pass0.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(SSAOPass0FS, "ssao_pass0.frag", Fragment);

	class SSAOBlurFS : public Shader
	{
		SHADER_TYPE(SSAOBlurFS)

		SSAOBlurFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
			Bind(samplePattern, "samplePattern");
			Bind(combineOutput, "combineOutput");
		}

		void SetSamplePattern(glm::vec2 pattern, bool combine)
		{
			Set(samplePattern, pattern);
			Set(combineOutput, combine);
		}

	private:
		Uniform samplePattern, combineOutput;
	};

	IMPLEMENT_SHADER_TYPE(SSAOBlurFS, "ssao_blur.frag", Fragment);

	struct SSAONoiseTexture
	{
		Texture tex;

		SSAONoiseTexture(int kernelWidth)
		{
			int samples = kernelWidth * kernelWidth;
			vector<glm::vec3> noise;

			std::mt19937 rng;
			std::uniform_real_distribution<float> dist(0.0f, 1.0f);

			for (int i = 0; i < samples; i++)
			{
				noise.push_back(normalize(glm::vec3{ dist(rng), dist(rng), 0.0f }));
			}

			tex.Create()
			.Filter(GL_NEAREST, GL_NEAREST).Wrap(GL_REPEAT, GL_REPEAT)
			.Size(kernelWidth, kernelWidth).Storage2D(PF_RGB8F).Image2D(noise.data());
		}
	};

	void SSAOPass0::Process(const PostProcessingContext *context)
	{
		static SSAONoiseTexture noiseTex(4);

		auto r = context->renderer;
		auto dest = outputs[0].AllocateTarget(context)->GetTexture();

		r->GlobalShaders->Get<SSAOPass0FS>()->SetProjection(r->Projection);

		r->SetRenderTarget(&dest, nullptr);
		r->ShaderControl->BindPipeline<SSAOPass0VS, SSAOPass0FS>(r->GlobalShaders);

		noiseTex.tex.Bind(3);

		DrawScreenCover();
	}

	void SSAOBlur::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto dest = outputs[0].AllocateTarget(context)->GetTexture();
		auto extent = GetInput(0)->GetOutput()->TargetDesc.extent;

		glm::vec2 samplePattern;

		if (horizontal)
			samplePattern.x = 1.0f / (float) extent.x;
		else
			samplePattern.y = 1.0f / (float) extent.y;

		r->GlobalShaders->Get<SSAOBlurFS>()->SetSamplePattern(samplePattern, !horizontal);

		r->SetRenderTarget(&dest, nullptr);
		r->ShaderControl->BindPipeline<BasicPostVS, SSAOBlurFS>(r->GlobalShaders);

		DrawScreenCover();
	}
}
