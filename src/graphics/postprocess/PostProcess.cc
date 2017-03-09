#include "graphics/postprocess/PostProcess.hh"
#include "graphics/postprocess/Helpers.hh"

#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/RenderTargetPool.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/GPUTimer.hh"
#include "graphics/Util.hh"

#include "graphics/postprocess/Bloom.hh"
#include "graphics/postprocess/GammaCorrect.hh"
#include "graphics/postprocess/Lighting.hh"
#include "graphics/postprocess/MenuGui.hh"
#include "graphics/postprocess/SSAO.hh"
#include "graphics/postprocess/SMAA.hh"
#include "graphics/postprocess/ViewGBuffer.hh"

#include "core/CVar.hh"

namespace sp
{
	static CVar<bool> CVarLightingEnabled("r.Lighting", true, "Enable lighting");
	static CVar<bool> CVarTonemapEnabled("r.Tonemap", true, "Enable HDR tonemapping");
	static CVar<bool> CVarBloomEnabled("r.Bloom", true, "Enable HDR bloom");
	static CVar<bool> CVarSSAOEnabled("r.SSAO", true, "Enable Screen Space Ambient Occlusion");
	static CVar<int> CVarViewGBuffer("r.ViewGBuffer", 0, "Show GBuffer (1: baseColor, 2: normal, 3: depth (or alpha), 4: roughness, 5: metallic (or radiance), 6: position, 7: face normal)");
	static CVar<int> CVarViewGBufferSource("r.ViewGBufferSource", 0, "GBuffer Debug Source (0: gbuffer, 1: voxel grid, 2: cone trace)");
	static CVar<int> CVarVoxelMip("r.VoxelMip", 0, "");
	static CVar<int> CVarAntiAlias("r.AntiAlias", 1, "Anti-aliasing mode (0: none, 1: SMAA 1x)");

	static void AddSSAO(PostProcessingContext &context)
	{
		auto ssaoPass0 = context.AddPass<SSAOPass0>();
		ssaoPass0->SetInput(0, context.GBuffer1);
		ssaoPass0->SetInput(1, context.GBuffer2);
		ssaoPass0->SetInput(2, context.MirrorIndexStencil);

		auto ssaoBlurX = context.AddPass<SSAOBlur>(true);
		ssaoBlurX->SetInput(0, ssaoPass0);
		ssaoBlurX->SetInput(1, context.Depth);

		auto ssaoBlurY = context.AddPass<SSAOBlur>(false);
		ssaoBlurY->SetInput(0, ssaoBlurX);
		ssaoBlurY->SetInput(1, context.Depth);

		context.AoBuffer = ssaoBlurY;
	}

	static void AddLighting(PostProcessingContext &context, VoxelData voxelData)
	{
		auto indirectDiffuse = context.AddPass<VoxelLightingDiffuse>(voxelData);
		indirectDiffuse->SetInput(0, context.GBuffer0);
		indirectDiffuse->SetInput(1, context.GBuffer1);
		indirectDiffuse->SetInput(2, context.GBuffer2);
		indirectDiffuse->SetInput(3, context.VoxelRadiance);

		auto lighting = context.AddPass<VoxelLighting>(voxelData, CVarSSAOEnabled.Get());
		lighting->SetInput(0, context.GBuffer0);
		lighting->SetInput(1, context.GBuffer1);
		lighting->SetInput(2, context.GBuffer2);
		lighting->SetInput(3, context.ShadowMap);
		lighting->SetInput(4, context.MirrorShadowMap);
		lighting->SetInput(5, context.VoxelRadiance);
		lighting->SetInput(6, indirectDiffuse);
		lighting->SetInput(7, context.MirrorIndexStencil);
		lighting->SetInput(8, context.LightingGel);
		lighting->SetInput(9, context.AoBuffer);

		context.LastOutput = lighting;
	}

	static void AddBloom(PostProcessingContext &context)
	{
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

	static void AddSMAA(PostProcessingContext &context, ProcessPassOutputRef linearLuminosity)
	{
		auto gammaCorrect = context.AddPass<GammaCorrect>();
		gammaCorrect->SetInput(0, linearLuminosity);

		auto edgeDetect = context.AddPass<SMAAEdgeDetection>();
		edgeDetect->SetInput(0, gammaCorrect);

		auto blendingWeights = context.AddPass<SMAABlendingWeights>();
		blendingWeights->SetInput(0, { edgeDetect, 0 });
		blendingWeights->SetDependency(0, { edgeDetect, 1 });

		auto blending = context.AddPass<SMAABlending>();
		blending->SetInput(0, context.LastOutput);
		blending->SetInput(1, blendingWeights);

		context.LastOutput = blending;
	}

	static void AddMenu(PostProcessingContext &context)
	{
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

	void PostProcessing::Process(Renderer *renderer, sp::Game *game, ecs::View view, const EngineRenderTargets &targets)
	{
		RenderPhase phase("PostProcessing", renderer->Timer);

		PostProcessingContext context;
		context.renderer = renderer;
		context.game = game;
		context.view = view;

		context.GBuffer0 = context.AddPass<ProxyProcessPass>(targets.gBuffer0);
		context.GBuffer1 = context.AddPass<ProxyProcessPass>(targets.gBuffer1);
		context.GBuffer2 = context.AddPass<ProxyProcessPass>(targets.gBuffer2);
		context.Depth = context.AddPass<ProxyProcessPass>(targets.depth);
		context.MirrorVisData = targets.mirrorVisData;
		context.MirrorSceneData = targets.mirrorSceneData;
		context.LastOutput = context.GBuffer0;

		if (targets.shadowMap)
		{
			context.ShadowMap = context.AddPass<ProxyProcessPass>(targets.shadowMap);
		}

		if (targets.mirrorShadowMap)
		{
			context.MirrorShadowMap = context.AddPass<ProxyProcessPass>(targets.mirrorShadowMap);
		}

		if (targets.voxelData.radiance)
		{
			context.VoxelRadiance = context.AddPass<ProxyProcessPass>(targets.voxelData.radiance);
		}

		if (targets.mirrorIndexStencil)
		{
			context.MirrorIndexStencil = context.AddPass<ProxyProcessPass>(targets.mirrorIndexStencil);
		}

		if (targets.lightingGel)
		{
			context.LightingGel = context.AddPass<ProxyProcessPass>(targets.lightingGel);
		}

		if (CVarSSAOEnabled.Get())
		{
			AddSSAO(context);
		}

		if (CVarLightingEnabled.Get() && targets.shadowMap != nullptr)
		{
			AddLighting(context, targets.voxelData);
		}

		auto linearLuminosity = context.LastOutput;

		{
			auto hist = context.AddPass<LumiHistogram>();
			hist->SetInput(0, context.LastOutput);
			context.LastOutput = hist;
		}

		if (game->menuGui.RenderMode() == MenuRenderMode::Pause)
		{
			AddMenu(context);
		}

		if (CVarBloomEnabled.Get())
		{
			AddBloom(context);
		}

		if (CVarTonemapEnabled.Get())
		{
			auto tonemap = context.AddPass<Tonemap>();
			tonemap->SetInput(0, context.LastOutput);
			context.LastOutput = tonemap;
		}

		if (CVarAntiAlias.Get() == 1)
		{
			AddSMAA(context, linearLuminosity);
		}

		if (CVarViewGBuffer.Get() > 0)
		{
			auto viewGBuf = context.AddPass<ViewGBuffer>(CVarViewGBuffer.Get(), CVarViewGBufferSource.Get(), CVarVoxelMip.Get(), targets.voxelData);
			viewGBuf->SetInput(0, context.GBuffer0);
			viewGBuf->SetInput(1, context.GBuffer1);
			viewGBuf->SetInput(2, context.GBuffer2);
			viewGBuf->SetInput(3, context.Depth);
			viewGBuf->SetInput(4, context.VoxelRadiance);
			context.LastOutput = viewGBuf;
		}

		auto lastOutput = context.LastOutput.GetOutput();
		lastOutput->AddDependency();

		context.ProcessAllPasses();

		renderer->SetDefaultRenderTarget();
		renderer->ShaderControl->BindPipeline<BasicPostVS, ScreenCoverFS>(renderer->GlobalShaders);

		glViewport(view.offset.x, view.offset.y, view.extents.x, view.extents.y);

		lastOutput->TargetRef->GetTexture().Bind(0);
		DrawScreenCover();

		lastOutput->ReleaseDependency();
	}

	ProcessPassOutput *ProcessPassOutputRef::GetOutput()
	{
		if (pass == nullptr) return nullptr;
		return pass->GetOutput(outputIndex);
	}

	void PostProcessingContext::ProcessAllPasses()
	{
		for (auto pass : passes)
		{
			// Propagate dependencies.
			for (uint32 id = 0;; id++)
			{
				ProcessPassOutputRef *depend = pass->GetAllDependencies(id);
				if (!depend) break;

				auto dependOutput = depend->GetOutput();

				if (dependOutput)
					dependOutput->AddDependency();
			}

			// Calculate render target descriptions.
			for (uint32 id = 0;; id++)
			{
				ProcessPassOutput *output = pass->GetOutput(id);
				if (!output) break;

				output->TargetDesc = pass->GetOutputDesc(id);
			}
		}

		// Process in order.
		for (auto pass : passes)
		{
			RenderPhase phase(pass->Name());

			if (phase.name != "ProxyTarget")
			{
				phase.StartTimer(renderer->Timer);
			}

			// Set up inputs.
			for (uint32 id = 0;; id++)
			{
				ProcessPassOutputRef *input = pass->GetInput(id);
				if (!input) break;

				auto inputOutput = input->GetOutput();

				if (inputOutput)
				{
					Assert(!!inputOutput->TargetRef, "post processing input is destroyed");
					inputOutput->TargetRef->GetTexture().Bind(id);
				}
			}

			//Debugf("Process %s", pass->Name());
			pass->Process(this);

			// Release dependencies.
			for (uint32 id = 0;; id++)
			{
				ProcessPassOutputRef *depend = pass->GetAllDependencies(id);
				if (!depend) break;

				auto dependOutput = depend->GetOutput();

				if (dependOutput)
					dependOutput->ReleaseDependency();
			}
		}
	}

	RenderTarget::Ref ProcessPassOutput::AllocateTarget(const PostProcessingContext *context)
	{
		if (TargetRef == nullptr)
		{
			TargetRef = context->renderer->RTPool->Get(TargetDesc);
			//Debugf("Reserve target %d", TargetRef->GetID());
		}
		return TargetRef;
	}
}
