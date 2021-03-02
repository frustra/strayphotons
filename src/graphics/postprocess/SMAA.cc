#include "SMAA.hh"

#include "assets/AssetManager.hh"
#include "core/CVar.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/voxel_renderer/VoxelRenderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/Util.hh"

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

    void SMAAEdgeDetection::Process(const PostProcessingContext *context) {
        auto r = context->renderer;
        auto dest = outputs[0].AllocateTarget(context);
        auto stencil = outputs[1].AllocateTarget(context);

#ifndef DISABLE_SMAA
        r->GlobalShaders->Get<SMAAEdgeDetectionVS>()->SetViewParams(context->view);
        r->GlobalShaders->Get<SMAAEdgeDetectionFS>()->SetViewParams(context->view);

        r->SetRenderTarget(dest, stencil);
        r->ShaderControl->BindPipeline<SMAAEdgeDetectionVS, SMAAEdgeDetectionFS>();

        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, 1, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilMask(0xff);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        DrawScreenCover();

        glDisable(GL_STENCIL_TEST);
#endif
    }

    void SMAABlendingWeights::Process(const PostProcessingContext *context) {
#ifndef DISABLE_SMAA
        if (CVarSMAADebug.Get() >= 2) {
            SetOutputTarget(0, GetInput(0)->GetOutput()->TargetRef);
            return;
        }

        static GLTexture areaTex = GAssets.LoadTexture("textures/smaa/AreaTex.tga", 1);
        static GLTexture searchTex = GAssets.LoadTexture("textures/smaa/SearchTex.tga", 1);

        auto r = context->renderer;
        auto dest = outputs[0].AllocateTarget(context);
        auto stencil = dependencies[0].GetOutput()->TargetRef;

        r->GlobalShaders->Get<SMAABlendingWeightsVS>()->SetViewParams(context->view);
        r->GlobalShaders->Get<SMAABlendingWeightsFS>()->SetViewParams(context->view);

        r->SetRenderTarget(dest, stencil);
        r->ShaderControl->BindPipeline<SMAABlendingWeightsVS, SMAABlendingWeightsFS>();

        areaTex.Bind(1);
        searchTex.Bind(2);

        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_EQUAL, 1, 0xff);
        glStencilOp(GL_ZERO, GL_KEEP, GL_REPLACE);
        glStencilMask(0x00);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        DrawScreenCover();

        glDisable(GL_STENCIL_TEST);
#else
        SetOutputTarget(0, GetInput(0)->GetOutput()->TargetRef);
#endif
    }

    void SMAABlending::Process(const PostProcessingContext *context) {
#ifndef DISABLE_SMAA
        if (CVarSMAADebug.Get() >= 1) {
            SetOutputTarget(0, GetInput(1)->GetOutput()->TargetRef);
            return;
        }

        auto r = context->renderer;
        auto dest = outputs[0].AllocateTarget(context);

        r->GlobalShaders->Get<SMAABlendingVS>()->SetViewParams(context->view);
        r->GlobalShaders->Get<SMAABlendingFS>()->SetViewParams(context->view);

        r->SetRenderTarget(dest, nullptr);
        r->ShaderControl->BindPipeline<SMAABlendingVS, SMAABlendingFS>();

        DrawScreenCover();
#else
        SetOutputTarget(0, GetInput(0)->GetOutput()->TargetRef);
#endif
    }
} // namespace sp
