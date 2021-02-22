#include "MenuGui.hh"

#include "graphics/GenericShaders.hh"
#include "graphics/voxel_renderer/VoxelRenderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/Util.hh"

namespace sp {
    void RenderMenuGui::Process(const PostProcessingContext *context) {
        auto r = context->renderer;
        auto target = GetInput(0)->GetOutput()->TargetRef;
        auto blurred = GetInput(1)->GetOutput()->TargetRef;
        auto dest = outputs[0].AllocateTarget(context);

        glViewport(0, 0, target->GetDesc().extent.x, target->GetDesc().extent.y);

        blurred->GetTexture().Bind(0);
        r->ShaderControl->BindPipeline<BasicPostVS, ScreenCoverFS>();
        r->SetRenderTarget(dest, nullptr);
        DrawScreenCover();

        auto view = context->view;
        r->RenderMainMenu(view);
    }
} // namespace sp
