#include "MenuGui.hh"

#include "graphics/opengl/GenericShaders.hh"
#include "graphics/opengl/ShaderManager.hh"
#include "graphics/opengl/voxel_renderer/VoxelRenderer.hh"

namespace sp {
    void RenderMenuGui::Process(const PostProcessingContext *context) {
        auto r = context->renderer;
        auto target = GetInput(0)->GetOutput()->TargetRef;
        auto blurred = GetInput(1)->GetOutput()->TargetRef;
        auto dest = outputs[0].AllocateTarget(context);

        glViewport(0, 0, target->GetDesc().extent.x, target->GetDesc().extent.y);

        blurred->GetGLTexture().Bind(0);
        r->ShaderControl->BindPipeline<BasicPostVS, ScreenCoverFS>();
        r->SetRenderTarget(dest.get(), nullptr);
        VoxelRenderer::DrawScreenCover();

        auto view = context->view;
        r->RenderMainMenu(view);
    }
} // namespace sp
