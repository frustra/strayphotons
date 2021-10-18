#include "Bloom.hh"

#include "console/CVar.hh"
#include "core/Logging.hh"
#include "graphics/opengl/GenericShaders.hh"
#include "graphics/opengl/ShaderManager.hh"
#include "graphics/opengl/voxel_renderer/VoxelRenderer.hh"

namespace sp {
    static CVar<float> CVarBloomWeight1("r.BloomWeight1", 0.4f, "Bloom kernel 1 weight");
    static CVar<float> CVarBloomWeight2("r.BloomWeight2", 0.5f, "Bloom kernel 2 weight");
    static CVar<float> CVarBloomScale("r.BloomScale", 0.15f, "Bloom prescale for highpass");

    class BloomHighpassFS : public Shader {
        SHADER_TYPE(BloomHighpassFS)
        using Shader::Shader;

        void SetScale(float newScale) {
            Set("scale", newScale);
        }
    };

    IMPLEMENT_SHADER_TYPE(BloomHighpassFS, "bloom_highpass.frag", Fragment);

    void BloomHighpass::Process(PostProcessLock lock, const PostProcessingContext *context) {
        auto r = context->renderer;
        auto dest = outputs[0].AllocateTarget(context);

        r->shaders.Get<BloomHighpassFS>()->SetScale(CVarBloomScale.Get());

        r->SetRenderTarget(dest.get(), nullptr);
        r->ShaderControl->BindPipeline<BasicPostVS, BloomHighpassFS>();

        auto desc = dest->GetDesc();
        glViewport(0, 0, desc.extent.x, desc.extent.y);
        VoxelRenderer::DrawScreenCover();
    }

    class BloomBlurFS : public Shader {
        SHADER_TYPE(BloomBlurFS)
        using Shader::Shader;

        void SetDirection(glm::ivec2 d) {
            Set("direction", glm::vec2(d));
        }

        void SetClip(float threshold, float scale) {
            Set("clip", glm::vec2(threshold, scale));
        }
    };

    IMPLEMENT_SHADER_TYPE(BloomBlurFS, "bloom_blur.frag", Fragment);

    void BloomBlur::Process(PostProcessLock lock, const PostProcessingContext *context) {
        auto r = context->renderer;
        auto dest = outputs[0].AllocateTarget(context);

        auto shader = r->shaders.Get<BloomBlurFS>();
        shader->SetDirection(direction);
        shader->SetClip(clip, scale);

        r->SetRenderTarget(dest.get(), nullptr);
        r->ShaderControl->BindPipeline<BasicPostVS, BloomBlurFS>();

        auto desc = dest->GetDesc();
        glViewport(0, 0, desc.extent.x, desc.extent.y);
        VoxelRenderer::DrawScreenCover();
    }

    class BloomCombineFS : public Shader {
        SHADER_TYPE(BloomCombineFS)
        using Shader::Shader;

        void SetWeights(float w1, float w2) {
            Set("weight1", w1);
            Set("weight2", w2);
        }
    };

    IMPLEMENT_SHADER_TYPE(BloomCombineFS, "bloom_combine.frag", Fragment);

    void BloomCombine::Process(PostProcessLock lock, const PostProcessingContext *context) {
        auto r = context->renderer;
        auto dest = outputs[0].AllocateTarget(context);

        r->shaders.Get<BloomCombineFS>()->SetWeights(CVarBloomWeight1.Get(), CVarBloomWeight2.Get());

        r->SetRenderTarget(dest.get(), nullptr);
        r->ShaderControl->BindPipeline<BasicPostVS, BloomCombineFS>();

        auto desc = dest->GetDesc();
        glViewport(0, 0, desc.extent.x, desc.extent.y);
        VoxelRenderer::DrawScreenCover();
    }
} // namespace sp
