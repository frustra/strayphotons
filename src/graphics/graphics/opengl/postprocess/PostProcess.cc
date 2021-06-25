#include "PostProcess.hh"

#include "core/CFunc.hh"
#include "core/CVar.hh"
#include "ecs/Ecs.hh"
#include "graphics/opengl/GLRenderTarget.hh"
#include "graphics/opengl/GenericShaders.hh"
#include "graphics/opengl/PerfTimer.hh"
#include "graphics/opengl/RenderTargetPool.hh"
#include "graphics/opengl/ShaderManager.hh"
#include "graphics/opengl/postprocess/Bloom.hh"
#include "graphics/opengl/postprocess/Crosshair.hh"
#include "graphics/opengl/postprocess/GammaCorrect.hh"
#include "graphics/opengl/postprocess/Helpers.hh"
#include "graphics/opengl/postprocess/Lighting.hh"
#include "graphics/opengl/postprocess/MenuGui.hh"
#include "graphics/opengl/postprocess/SMAA.hh"
#include "graphics/opengl/postprocess/SSAO.hh"
#include "graphics/opengl/postprocess/ViewGBuffer.hh"
#include "graphics/opengl/voxel_renderer/VoxelRenderer.hh"

#include <filesystem>
#include <stb_image_write.h>

namespace sp {
    // clang-format off
    static CVar<bool> CVarLightingEnabled("r.Lighting", true, "Enable lighting");
    static CVar<bool> CVarTonemapEnabled("r.Tonemap", true, "Enable HDR tonemapping");
    static CVar<bool> CVarBloomEnabled("r.Bloom", true, "Enable HDR bloom");
    static CVar<bool> CVarSSAOEnabled("r.SSAO", false, "Enable Screen Space Ambient Occlusion");
    static CVar<int> CVarViewGBuffer("r.ViewGBuffer", 0, "Show GBuffer (1: baseColor, 2: normal, 3: depth (or alpha), 4: roughness, 5: metallic (or radiance), 6: position, 7: face normal)");
    static CVar<int> CVarViewGBufferSource("r.ViewGBufferSource", 0, "GBuffer Debug Source (0: gbuffer, 1: voxel grid, 2: cone trace)");
    static CVar<int> CVarVoxelMip("r.VoxelMip", 0, "");
    static CVar<int> CVarAntiAlias("r.AntiAlias", 1, "Anti-aliasing mode (0: none, 1: SMAA 1x)");
    // clang-format on

    static void AddSSAO(PostProcessingContext &context) {
        auto ssaoPass0 = context.AddPass<SSAOPass0>();
        ssaoPass0->SetInput(0, context.GBuffer1);
        ssaoPass0->SetInput(1, context.GBuffer2);
        ssaoPass0->SetInput(2, context.MirrorIndexStencil);

        auto ssaoBlurX = context.AddPass<SSAOBlur>(true);
        ssaoBlurX->SetInput(0, ssaoPass0);
        ssaoBlurX->SetInput(1, context.GBuffer2);

        auto ssaoBlurY = context.AddPass<SSAOBlur>(false);
        ssaoBlurY->SetInput(0, ssaoBlurX);
        ssaoBlurY->SetInput(1, context.GBuffer2);

        context.AoBuffer = ssaoBlurY;
    }

    static void AddLighting(PostProcessingContext &context, VoxelData voxelData) {
        auto indirectDiffuse = context.AddPass<VoxelLightingDiffuse>(voxelData);
        indirectDiffuse->SetInput(0, context.GBuffer0);
        indirectDiffuse->SetInput(1, context.GBuffer1);
        indirectDiffuse->SetInput(2, context.GBuffer2);
        indirectDiffuse->SetInput(3, context.VoxelRadiance);
        indirectDiffuse->SetInput(4, context.VoxelRadianceMips);

        auto lighting = context.AddPass<VoxelLighting>(voxelData, CVarSSAOEnabled.Get());
        lighting->SetInput(0, context.GBuffer0);
        lighting->SetInput(1, context.GBuffer1);
        lighting->SetInput(2, context.GBuffer2);
        lighting->SetInput(3, context.GBuffer3);
        lighting->SetInput(4, context.ShadowMap);
        lighting->SetInput(5, context.MirrorShadowMap);
        lighting->SetInput(6, context.VoxelRadiance);
        lighting->SetInput(7, context.VoxelRadianceMips);
        lighting->SetInput(8, indirectDiffuse);
        lighting->SetInput(9, context.MirrorIndexStencil);
        lighting->SetInput(10, context.LightingGel);
        lighting->SetInput(11, context.AoBuffer);

        context.LastOutput = lighting;
    }

    static void AddBloom(PostProcessingContext &context) {
        auto highpass = context.AddPass<BloomHighpass>();
        highpass->SetInput(0, context.LastOutput);

        auto blurY1 = context.AddPass<BloomBlur>(glm::ivec2(0, 1), 1);
        blurY1->SetInput(0, highpass);

        auto blurX1 = context.AddPass<BloomBlur>(glm::ivec2(1, 0), 2);
        blurX1->SetInput(0, blurY1);

        auto blurY2 = context.AddPass<BloomBlur>(glm::ivec2(0, 1), 1);
        blurY2->SetInput(0, blurX1);

        auto blurX2 = context.AddPass<BloomBlur>(glm::ivec2(1, 0), 1);
        blurX2->SetInput(0, blurY2);

        auto combine = context.AddPass<BloomCombine>();
        combine->SetInput(0, context.LastOutput);
        combine->SetInput(1, blurX1);
        combine->SetInput(2, blurX2);

        context.LastOutput = combine;
    }

    static void AddSMAA(PostProcessingContext &context, ProcessPassOutputRef linearLuminosity) {
        auto gammaCorrect = context.AddPass<GammaCorrect>();
        gammaCorrect->SetInput(0, linearLuminosity);

        auto edgeDetect = context.AddPass<SMAAEdgeDetection>();
        edgeDetect->SetInput(0, gammaCorrect);

        auto blendingWeights = context.AddPass<SMAABlendingWeights>();
        blendingWeights->SetInput(0, {edgeDetect, 0});
        blendingWeights->SetDependency(0, {edgeDetect, 1});

        auto blending = context.AddPass<SMAABlending>();
        blending->SetInput(0, context.LastOutput);
        blending->SetInput(1, blendingWeights);

        context.LastOutput = blending;
    }

    static void AddMenu(PostProcessingContext &context) {
        auto blurY1 = context.AddPass<BloomBlur>(glm::ivec2(0, 1), 2, 1.0f);
        blurY1->SetInput(0, context.LastOutput);

        auto blurX1 = context.AddPass<BloomBlur>(glm::ivec2(1, 0), 2);
        blurX1->SetInput(0, blurY1);

        auto blurY2 = context.AddPass<BloomBlur>(glm::ivec2(0, 1), 1);
        blurY2->SetInput(0, blurX1);

        auto blurX2 = context.AddPass<BloomBlur>(glm::ivec2(1, 0), 2);
        blurX2->SetInput(0, blurY2);

        auto blurY3 = context.AddPass<BloomBlur>(glm::ivec2(0, 1), 1);
        blurY3->SetInput(0, blurX2);

        auto blurX3 = context.AddPass<BloomBlur>(glm::ivec2(1, 0), 1, FLT_MAX, 0.2f);
        blurX3->SetInput(0, blurY3);

        auto menu = context.AddPass<RenderMenuGui>();
        menu->SetInput(0, context.LastOutput);
        menu->SetInput(1, blurX3);
        context.LastOutput = menu;
    }

    static string ScreenshotPath;

    CFunc<string> CFuncQueueScreenshot("screenshot", "Save screenshot to <path>", [](string path) {
        if (ScreenshotPath.empty()) {
            ScreenshotPath = path;
        } else {
            Logf("Can't save multiple screenshots on the same frame: %s, already saving %s",
                 path.c_str(),
                 ScreenshotPath.c_str());
        }
    });

    void SaveScreenshot(string path, GLTexture &tex) {
        auto base = std::filesystem::absolute("screenshots");
        if (!std::filesystem::is_directory(base)) {
            if (!std::filesystem::create_directory(base)) {
                Errorf("Couldn't save screenshot, couldn't create output directory: %s", base.c_str());
                return;
            }
        }
        auto fullPath = std::filesystem::weakly_canonical(base / path);
        Logf("Saving screenshot to: %s", fullPath.c_str());

        size_t size = tex.width * tex.height * 4;
        uint8 *buf = new uint8[size], *flipped = new uint8[size];
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glGetTextureImage(tex.handle, 0, GL_RGBA, GL_UNSIGNED_BYTE, size, buf);

        for (int y = 0; y < tex.height; y++) {
            memcpy(flipped + tex.width * (tex.height - y - 1) * 4, buf + tex.width * y * 4, tex.width * 4);
        }

        stbi_write_png((const char *)fullPath.c_str(), tex.width, tex.height, 4, flipped, 0);

        delete[] buf;
        delete[] flipped;
    }

    void PostProcessing::Process(VoxelRenderer *renderer,
                                 ecs::EntityManager &ecs,
                                 ecs::View view,
                                 const EngineRenderTargets &targets) {
        RenderPhase phase("PostProcessing", renderer->Timer);

        bool renderToTexture = (targets.finalOutput != nullptr);

        PostProcessingContext context(renderer, ecs, view);

        context.GBuffer0 = context.AddPass<ProxyProcessPass>(targets.gBuffer0);
        context.GBuffer1 = context.AddPass<ProxyProcessPass>(targets.gBuffer1);
        context.GBuffer2 = context.AddPass<ProxyProcessPass>(targets.gBuffer2);
        context.GBuffer3 = context.AddPass<ProxyProcessPass>(targets.gBuffer3);
        context.MirrorVisData = targets.mirrorVisData;
        context.MirrorSceneData = targets.mirrorSceneData;
        context.LastOutput = context.GBuffer0;

        if (targets.shadowMap) { context.ShadowMap = context.AddPass<ProxyProcessPass>(targets.shadowMap); }

        if (targets.mirrorShadowMap) {
            context.MirrorShadowMap = context.AddPass<ProxyProcessPass>(targets.mirrorShadowMap);
        }

        if (targets.voxelData.radiance && targets.voxelData.radianceMips) {
            context.VoxelRadiance = context.AddPass<ProxyProcessPass>(targets.voxelData.radiance);
            context.VoxelRadianceMips = context.AddPass<ProxyProcessPass>(targets.voxelData.radianceMips);
        }

        if (targets.mirrorIndexStencil) {
            context.MirrorIndexStencil = context.AddPass<ProxyProcessPass>(targets.mirrorIndexStencil);
        }

        if (targets.lightingGel) { context.LightingGel = context.AddPass<ProxyProcessPass>(targets.lightingGel); }

        if (CVarSSAOEnabled.Get()) { AddSSAO(context); }

        if (CVarLightingEnabled.Get() && targets.shadowMap != nullptr) { AddLighting(context, targets.voxelData); }

        auto linearLuminosity = context.LastOutput;

        {
            auto hist = context.AddPass<LumiHistogram>();
            hist->SetInput(0, context.LastOutput);
            context.LastOutput = hist;
        }

        // TODO: Update gui rendering to use the ECS
        if (!renderToTexture && renderer->GetMenuRenderMode() == MenuRenderMode::Pause) { AddMenu(context); }

        if (CVarBloomEnabled.Get()) { AddBloom(context); }

        if (CVarTonemapEnabled.Get()) {
            auto tonemap = context.AddPass<Tonemap>();
            tonemap->SetInput(0, context.LastOutput);
            context.LastOutput = tonemap;
        }

        if (CVarAntiAlias.Get() == 1) { AddSMAA(context, linearLuminosity); }

        if (!renderToTexture && renderer->GetMenuRenderMode() == MenuRenderMode::None) {
            auto crosshair = context.AddPass<Crosshair>();
            crosshair->SetInput(0, context.LastOutput);
            context.LastOutput = crosshair;
        }

        if (CVarViewGBuffer.Get() > 0 && (renderer->GetMenuRenderMode() == MenuRenderMode::None)) {
            auto viewGBuf = context.AddPass<ViewGBuffer>(CVarViewGBuffer.Get(),
                                                         CVarViewGBufferSource.Get(),
                                                         CVarVoxelMip.Get(),
                                                         targets.voxelData);
            viewGBuf->SetInput(0, context.GBuffer0);
            viewGBuf->SetInput(1, context.GBuffer1);
            viewGBuf->SetInput(2, context.GBuffer2);
            viewGBuf->SetInput(3, context.GBuffer3);
            viewGBuf->SetInput(4, context.VoxelRadiance);
            viewGBuf->SetInput(5, context.VoxelRadianceMips);
            context.LastOutput = viewGBuf;
        }

        auto lastOutput = context.LastOutput.GetOutput();
        lastOutput->AddDependency();

        context.ProcessAllPasses();

        if (renderToTexture) {
            renderer->SetRenderTarget((GLRenderTarget *)targets.finalOutput, nullptr);
        } else {
            renderer->SetDefaultRenderTarget();
        }
        renderer->ShaderControl->BindPipeline<BasicPostVS, ScreenCoverFS>();

        glViewport(view.offset.x, view.offset.y, view.extents.x * view.scale, view.extents.y * view.scale);

        lastOutput->TargetRef->GetTexture().Bind(0);
        VoxelRenderer::DrawScreenCover();

        if (!ScreenshotPath.empty()) {
            SaveScreenshot(ScreenshotPath, lastOutput->TargetRef->GetTexture());
            ScreenshotPath = "";
        }

        lastOutput->ReleaseDependency();
    }

    ProcessPassOutput *ProcessPassOutputRef::GetOutput() {
        if (pass == nullptr) return nullptr;
        return pass->GetOutput(outputIndex);
    }

    void PostProcessingContext::ProcessAllPasses() {
        for (auto pass : passes) {
            // Propagate dependencies.
            for (uint32 id = 0;; id++) {
                ProcessPassOutputRef *depend = pass->GetAllDependencies(id);
                if (!depend) break;

                auto dependOutput = depend->GetOutput();

                if (dependOutput) dependOutput->AddDependency();
            }

            // Calculate render target descriptions.
            for (uint32 id = 0;; id++) {
                ProcessPassOutput *output = pass->GetOutput(id);
                if (!output) break;

                output->TargetDesc = pass->GetOutputDesc(id);
            }
        }

        // Process in order.
        for (auto pass : passes) {
            RenderPhase phase(pass->Name());

            if (phase.name != "ProxyTarget") { phase.StartTimer(renderer->Timer); }

            // Set up inputs.
            for (uint32 id = 0;; id++) {
                ProcessPassOutputRef *input = pass->GetInput(id);
                if (!input) break;

                auto inputOutput = input->GetOutput();

                if (inputOutput) {
                    Assert(!!inputOutput->TargetRef, "post processing input is destroyed");
                    inputOutput->TargetRef->GetTexture().Bind(id);
                }
            }

            // Debugf("Process %s", pass->Name());
            pass->Process(this);

            // Release dependencies.
            for (uint32 id = 0;; id++) {
                ProcessPassOutputRef *depend = pass->GetAllDependencies(id);
                if (!depend) break;

                auto dependOutput = depend->GetOutput();

                if (dependOutput) dependOutput->ReleaseDependency();
            }
        }
    }

    std::shared_ptr<GLRenderTarget> ProcessPassOutput::AllocateTarget(const PostProcessingContext *context) {
        if (TargetRef == nullptr) {
            TargetRef = context->renderer->RTPool->Get(TargetDesc);
            // Debugf("Reserve target %d", TargetRef->GetID());
        }
        return TargetRef;
    }
} // namespace sp
