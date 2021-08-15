#include "ViewGBuffer.hh"

#include "graphics/opengl/GPUTypes.hh"
#include "graphics/opengl/GenericShaders.hh"
#include "graphics/opengl/ShaderManager.hh"
#include "graphics/opengl/voxel_renderer/VoxelRenderer.hh"

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

    void ViewGBuffer::Process(PostProcessLock lock, const PostProcessingContext *context) {
        auto r = context->renderer;
        auto dest = outputs[0].AllocateTarget(context);

        GLVoxelInfo voxelInfo;
        FillVoxelInfo(&voxelInfo, voxelContext);

        r->shaders.Get<ViewGBufferFS>()->SetParameters(mode, source, level, context->view);
        r->shaders.Get<ViewGBufferFS>()->SetVoxelInfo(&voxelInfo);

        r->SetRenderTarget(dest.get(), nullptr);
        r->ShaderControl->BindPipeline<BasicPostVS, ViewGBufferFS>();

        VoxelRenderer::DrawScreenCover();
    }
} // namespace sp
