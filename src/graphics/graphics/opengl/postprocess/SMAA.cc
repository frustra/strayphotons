#include "SMAA.hh"

#include "assets/AssetManager.hh"
#include "core/CVar.hh"
#include "graphics/opengl/GenericShaders.hh"
#include "graphics/opengl/GlfwGraphicsContext.hh"
#include "graphics/opengl/ShaderManager.hh"
#include "graphics/opengl/voxel_renderer/VoxelRenderer.hh"

//#define DISABLE_SMAA

namespace sp {
#ifndef DISABLE_SMAA
    static CVar<int> CVarSMAADebug("r.SMAADebug", 0, "Show SMAA intermediates (1: weights, 2: edges)");
#endif

    class SMAAShaderBase : public Shader {
    public:
        using Shader::Shader;

        void SetViewParams(const ecs::View &view) {
            auto extents = glm::vec2(view.extents);
            glm::vec4 metrics(1.0f / extents, extents);
            Set("smaaRTMetrics", metrics);
        }
    };

    class SMAAEdgeDetectionVS : public SMAAShaderBase {
        SHADER_TYPE(SMAAEdgeDetectionVS)
        using SMAAShaderBase::SMAAShaderBase;
    };

    class SMAAEdgeDetectionFS : public SMAAShaderBase {
        SHADER_TYPE(SMAAEdgeDetectionFS)
        using SMAAShaderBase::SMAAShaderBase;
    };

    class SMAABlendingWeightsVS : public SMAAShaderBase {
        SHADER_TYPE(SMAABlendingWeightsVS)
        using SMAAShaderBase::SMAAShaderBase;
    };

    class SMAABlendingWeightsFS : public SMAAShaderBase {
        SHADER_TYPE(SMAABlendingWeightsFS)
        using SMAAShaderBase::SMAAShaderBase;
    };

    class SMAABlendingVS : public SMAAShaderBase {
        SHADER_TYPE(SMAABlendingVS)
        using SMAAShaderBase::SMAAShaderBase;
    };

    class SMAABlendingFS : public SMAAShaderBase {
        SHADER_TYPE(SMAABlendingFS)
        using SMAAShaderBase::SMAAShaderBase;
    };

#ifndef DISABLE_SMAA
    IMPLEMENT_SHADER_TYPE(SMAAEdgeDetectionVS, "smaa/edge_detection.vert", Vertex);
    IMPLEMENT_SHADER_TYPE(SMAAEdgeDetectionFS, "smaa/edge_detection.frag", Fragment);
    IMPLEMENT_SHADER_TYPE(SMAABlendingWeightsVS, "smaa/blending_weights.vert", Vertex);
    IMPLEMENT_SHADER_TYPE(SMAABlendingWeightsFS, "smaa/blending_weights.frag", Fragment);
    IMPLEMENT_SHADER_TYPE(SMAABlendingVS, "smaa/blending.vert", Vertex);
    IMPLEMENT_SHADER_TYPE(SMAABlendingFS, "smaa/blending.frag", Fragment);
#endif

    void SMAAEdgeDetection::Process(PostProcessLock lock, const PostProcessingContext *context) {
        auto r = context->renderer;
        auto dest = outputs[0].AllocateTarget(context);
        auto stencil = outputs[1].AllocateTarget(context);

#ifndef DISABLE_SMAA
        r->shaders.Get<SMAAEdgeDetectionVS>()->SetViewParams(context->view);
        r->shaders.Get<SMAAEdgeDetectionFS>()->SetViewParams(context->view);

        r->SetRenderTarget(dest.get(), stencil.get());
        r->ShaderControl->BindPipeline<SMAAEdgeDetectionVS, SMAAEdgeDetectionFS>();

        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, 1, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilMask(0xff);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        VoxelRenderer::DrawScreenCover();

        glDisable(GL_STENCIL_TEST);
#endif
    }

    void SMAABlendingWeights::Process(PostProcessLock lock, const PostProcessingContext *context) {
#ifndef DISABLE_SMAA
        if (CVarSMAADebug.Get() >= 2) {
            SetOutputTarget(0, GetInput(0)->GetOutput()->TargetRef);
            return;
        }

        static std::shared_ptr<GpuTexture> areaTex = context->renderer->context.LoadTexture(
            GAssets.LoadImage("textures/smaa/AreaTex.tga"),
            false);
        static std::shared_ptr<GpuTexture> searchTex = context->renderer->context.LoadTexture(
            GAssets.LoadImage("textures/smaa/SearchTex.tga"),
            false);
        static GLTexture *glAreaTex = dynamic_cast<GLTexture *>(areaTex.get());
        static GLTexture *glSearchTex = dynamic_cast<GLTexture *>(searchTex.get());
        Assert(glAreaTex != nullptr, "Failed to load SMAA glAreaTex");
        Assert(glSearchTex != nullptr, "Failed to load SMAA glSearchTex");

        auto r = context->renderer;
        auto dest = outputs[0].AllocateTarget(context);
        auto stencil = dependencies[0].GetOutput()->TargetRef;

        r->shaders.Get<SMAABlendingWeightsVS>()->SetViewParams(context->view);
        r->shaders.Get<SMAABlendingWeightsFS>()->SetViewParams(context->view);

        r->SetRenderTarget(dest.get(), stencil.get());
        r->ShaderControl->BindPipeline<SMAABlendingWeightsVS, SMAABlendingWeightsFS>();

        glAreaTex->Bind(1);
        glSearchTex->Bind(2);

        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_EQUAL, 1, 0xff);
        glStencilOp(GL_ZERO, GL_KEEP, GL_REPLACE);
        glStencilMask(0x00);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        VoxelRenderer::DrawScreenCover();

        glDisable(GL_STENCIL_TEST);
#else
        SetOutputTarget(0, GetInput(0)->GetOutput()->TargetRef);
#endif
    }

    void SMAABlending::Process(PostProcessLock lock, const PostProcessingContext *context) {
#ifndef DISABLE_SMAA
        if (CVarSMAADebug.Get() >= 1) {
            SetOutputTarget(0, GetInput(1)->GetOutput()->TargetRef);
            return;
        }

        auto r = context->renderer;
        auto dest = outputs[0].AllocateTarget(context);

        r->shaders.Get<SMAABlendingVS>()->SetViewParams(context->view);
        r->shaders.Get<SMAABlendingFS>()->SetViewParams(context->view);

        r->SetRenderTarget(dest.get(), nullptr);
        r->ShaderControl->BindPipeline<SMAABlendingVS, SMAABlendingFS>();

        VoxelRenderer::DrawScreenCover();
#else
        SetOutputTarget(0, GetInput(0)->GetOutput()->TargetRef);
#endif
    }
} // namespace sp
