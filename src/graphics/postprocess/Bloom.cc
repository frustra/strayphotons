#include "Bloom.hh"
#include "core/Logging.hh"
#include "core/CVar.hh"
#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Util.hh"

namespace sp
{
	static CVar<float> CVarBloomWeight1("r.BloomWeight1", 0.4f, "Bloom kernel 1 weight");
	static CVar<float> CVarBloomWeight2("r.BloomWeight2", 0.5f, "Bloom kernel 2 weight");
	static CVar<float> CVarBloomScale("r.BloomScale", 0.15f, "Bloom prescale for highpass");

	class BloomHighpassFS : public Shader
	{
		SHADER_TYPE(BloomHighpassFS)

		BloomHighpassFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
			Bind(scale, "scale");
		}

		void SetScale(float newScale)
		{
			Set(scale, newScale);
		}

	private:
		Uniform scale;
	};

	IMPLEMENT_SHADER_TYPE(BloomHighpassFS, "bloom_highpass.frag", Fragment);

	void BloomHighpass::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto dest = outputs[0].AllocateTarget(context);

		r->GlobalShaders->Get<BloomHighpassFS>()->SetScale(CVarBloomScale.Get());

		r->SetRenderTarget(dest, nullptr);
		r->ShaderControl->BindPipeline<BasicPostVS, BloomHighpassFS>(r->GlobalShaders);

		auto desc = dest->GetDesc();
		glViewport(0, 0, desc.extent.x, desc.extent.y);
		DrawScreenCover();
	}

	class BloomBlurFS : public Shader
	{
		SHADER_TYPE(BloomBlurFS)

		BloomBlurFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
			Bind(direction, "direction");
			Bind(clip, "clip");
		}

		void SetDirection(glm::ivec2 d)
		{
			Set(direction, glm::vec2(d));
		}

		void SetClip(float threshold, float scale)
		{
			Set(clip, glm::vec2(threshold, scale));
		}

	private:
		Uniform direction, clip;
	};

	IMPLEMENT_SHADER_TYPE(BloomBlurFS, "bloom_blur.frag", Fragment);

	void BloomBlur::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto dest = outputs[0].AllocateTarget(context);

		auto shader = r->GlobalShaders->Get<BloomBlurFS>();
		shader->SetDirection(direction);
		shader->SetClip(clip, scale);

		r->SetRenderTarget(dest, nullptr);
		r->ShaderControl->BindPipeline<BasicPostVS, BloomBlurFS>(r->GlobalShaders);

		auto desc = dest->GetDesc();
		glViewport(0, 0, desc.extent.x, desc.extent.y);
		DrawScreenCover();
	}

	class BloomCombineFS : public Shader
	{
		SHADER_TYPE(BloomCombineFS)

		BloomCombineFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
			Bind(weight1, "weight1");
			Bind(weight2, "weight2");
		}

		void SetWeights(float w1, float w2)
		{
			Set(weight1, w1);
			Set(weight2, w2);
		}

	private:
		Uniform weight1, weight2;
	};

	IMPLEMENT_SHADER_TYPE(BloomCombineFS, "bloom_combine.frag", Fragment);

	void BloomCombine::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto dest = outputs[0].AllocateTarget(context);

		r->GlobalShaders->Get<BloomCombineFS>()->SetWeights(CVarBloomWeight1.Get(), CVarBloomWeight2.Get());

		r->SetRenderTarget(dest, nullptr);
		r->ShaderControl->BindPipeline<BasicPostVS, BloomCombineFS>(r->GlobalShaders);

		auto desc = dest->GetDesc();
		glViewport(0, 0, desc.extent.x, desc.extent.y);
		DrawScreenCover();
	}
}
