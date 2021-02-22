#include "ViewGBuffer.hh"

#include "graphics/GenericShaders.hh"
#include "graphics/voxel_renderer/VoxelRenderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/Util.hh"

namespace sp {
    class ViewGBufferFS : public Shader {
        SHADER_TYPE(ViewGBufferFS)

        ViewGBufferFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput) {
            BindBuffer(voxelInfo, 0);
        }

        void SetParameters(int newMode, int newSource, int newLevel, const ecs::View &view) {
            Set("mode", newMode);
            Set("source", newSource);
            Set("mipLevel", newLevel);
            Set("invProjMat", view.invProjMat);
            Set("invViewMat", view.invViewMat);
        }

        void SetVoxelInfo(GLVoxelInfo *data) {
            BufferData(voxelInfo, sizeof(GLVoxelInfo), data);
        }

    private:
        UniformBuffer voxelInfo;
    };

    IMPLEMENT_SHADER_TYPE(ViewGBufferFS, "view_gbuffer.frag", Fragment);

    void ViewGBuffer::Process(const PostProcessingContext *context) {
        auto r = context->renderer;
        auto dest = outputs[0].AllocateTarget(context);

        GLVoxelInfo voxelInfo;
        FillVoxelInfo(&voxelInfo, voxelData.info);

        r->GlobalShaders->Get<ViewGBufferFS>()->SetParameters(mode, source, level, context->view);
        r->GlobalShaders->Get<ViewGBufferFS>()->SetVoxelInfo(&voxelInfo);

        r->SetRenderTarget(dest, nullptr);
        r->ShaderControl->BindPipeline<BasicPostVS, ViewGBufferFS>();

        DrawScreenCover();
    }
} // namespace sp