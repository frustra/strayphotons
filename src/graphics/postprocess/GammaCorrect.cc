#include "GammaCorrect.hh"

#include "SMAA.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/voxel_renderer/VoxelRenderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/Util.hh"

namespace sp {
    class GammaCorrectFS : public Shader {
        SHADER_TYPE(GammaCorrectFS)
        using Shader::Shader;
    };

    IMPLEMENT_SHADER_TYPE(GammaCorrectFS, "gamma_correct.frag", Fragment);

    void GammaCorrect::Process(const PostProcessingContext *context) {
        auto r = context->renderer;
        auto dest = outputs[0].AllocateTarget(context);

        r->SetRenderTarget(dest, nullptr);
        r->ShaderControl->BindPipeline<BasicPostVS, GammaCorrectFS>();

        DrawScreenCover();
    }
} // namespace sp