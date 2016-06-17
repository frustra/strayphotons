#include "graphics/postprocess/PostProcess.hh"
#include "graphics/postprocess/Helpers.hh"

#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/RenderTargetPool.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/GPUTimer.hh"
#include "graphics/Util.hh"

#include "graphics/postprocess/Lighting.hh"
#include "graphics/postprocess/ViewGBuffer.hh"
#include "graphics/postprocess/SSAO.hh"
#include "graphics/postprocess/SMAA.hh"
#include "graphics/postprocess/GammaCorrect.hh"

#include "core/CVar.hh"

namespace sp
{
	static CVar<bool> CVarLightingEnabled("r.Lighting", true, "Enable lighting");
	static CVar<bool> CVarTonemapEnabled("r.Tonemap", true, "Enable HDR tonemapping");
	static CVar<bool> CVarSSAOEnabled("r.SSAO", true, "Enable Screen Space Ambient Occlusion");
	static CVar<int> CVarViewGBuffer("r.ViewGBuffer", 0, "Show GBuffer (1: baseColor, 2: normal, 3: depth, 4: roughness)");
	static CVar<int> CVarAntiAlias("r.AntiAlias", 1, "Anti-aliasing mode (0: none, 1: SMAA 1x)");

	static void AddSSAO(PostProcessingContext &context)
	{
		auto ssaoPass0 = context.AddPass<SSAOPass0>();
		ssaoPass0->SetInput(0, context.LastOutput);
		ssaoPass0->SetInput(1, context.GBuffer1);
		ssaoPass0->SetInput(2, context.Depth);

		auto ssaoBlurX = context.AddPass<SSAOBlur>(true);
		ssaoBlurX->SetInput(0, ssaoPass0);
		ssaoBlurX->SetInput(1, context.Depth);

		auto ssaoBlurY = context.AddPass<SSAOBlur>(false);
		ssaoBlurY->SetInput(0, ssaoBlurX);
		ssaoBlurY->SetInput(1, context.Depth);
		ssaoBlurY->SetInput(2, context.LastOutput);

		context.LastOutput = ssaoBlurY;
	}

	static void AddLighting(PostProcessingContext &context)
	{
		auto lighting = context.AddPass<DeferredLighting>();
		lighting->SetInput(0, context.LastOutput);
		lighting->SetInput(1, context.GBuffer1);
		lighting->SetInput(2, context.Depth);
		lighting->SetInput(3, context.ShadowMap);

		context.LastOutput = lighting;
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

	void PostProcessing::Process(Renderer *renderer, sp::Game *game, ecs::View view, const EngineRenderTargets &targets)
	{
		RenderPhase phase("PostProcessing", renderer->timer);

		PostProcessingContext context;
		context.renderer = renderer;
		context.game = game;
		context.view = view;

		context.GBuffer0 = context.AddPass<ProxyProcessPass>(targets.GBuffer0);
		context.GBuffer1 = context.AddPass<ProxyProcessPass>(targets.GBuffer1);
		context.Depth = context.AddPass<ProxyProcessPass>(targets.Depth);
		context.ShadowMap = context.AddPass<ProxyProcessPass>(targets.ShadowMap);
		context.LastOutput = context.GBuffer0;

		if (CVarLightingEnabled.Get())
		{
			AddLighting(context);
		}

		auto linearLuminosity = context.LastOutput;

		if (CVarSSAOEnabled.Get())
		{
			AddSSAO(context);
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
			auto viewGBuf = context.AddPass<ViewGBuffer>(CVarViewGBuffer.Get());
			viewGBuf->SetInput(0, context.GBuffer0);
			viewGBuf->SetInput(1, context.GBuffer1);
			viewGBuf->SetInput(2, context.Depth);
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
			RenderPhase phase(pass->Name(), renderer->timer);

			// Set up inputs.
			for (uint32 id = 0;; id++)
			{
				ProcessPassOutputRef *input = pass->GetInput(id);
				if (!input) break;

				auto inputOutput = input->GetOutput();

				if (inputOutput)
					inputOutput->TargetRef->GetTexture().Bind(id);
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
