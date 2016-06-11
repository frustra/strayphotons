#include "graphics/postprocess/PostProcess.hh"
#include "graphics/postprocess/Helpers.hh"

#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/RenderTargetPool.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Util.hh"

#include "graphics/postprocess/SSAO.hh"

#include "core/CVar.hh"

namespace sp
{
	static CVar<bool> CVarSSAOEnabled("r.SSAOEnabled", true, "Enable Screen Space Ambient Occlusion");
	static CVar<bool> CVarDebugSSAO("r.DebugSSAO", false, "Show unprocessed SSAO output");

	ProcessPassOutput *ProcessPassOutputRef::GetOutput()
	{
		return pass->GetOutput(outputIndex);
	}

	static void AddSSAO(PostProcessingContext &context)
	{
		auto ssaoPass0 = context.AddPass<SSAOPass0>();
		ssaoPass0->SetInput(0, context.LastOutput);
		ssaoPass0->SetInput(1, context.GBuffer1);
		ssaoPass0->SetInput(2, context.Depth);

		if (CVarDebugSSAO.Get())
		{
			context.LastOutput = ProcessPassOutputRef(ssaoPass0);
		}
		else
		{
			auto ssaoBlurX = context.AddPass<SSAOBlur>(true);
			ssaoBlurX->SetInput(0, { ssaoPass0 });
			ssaoBlurX->SetInput(1, context.GBuffer1);
			ssaoBlurX->SetInput(2, context.Depth);
			ssaoBlurX->SetInput(3, context.GBuffer0);

			auto ssaoBlurY = context.AddPass<SSAOBlur>(false);
			ssaoBlurY->SetInput(0, { ssaoBlurX });
			ssaoBlurY->SetInput(1, context.GBuffer1);
			ssaoBlurY->SetInput(2, context.Depth);
			ssaoBlurY->SetInput(3, context.GBuffer0);

			context.LastOutput = ProcessPassOutputRef(ssaoBlurY);
		}
	}

	void PostProcessing::Process(Renderer *renderer, const EngineRenderTargets &targets)
	{
		PostProcessingContext context;
		context.renderer = renderer;

		context.GBuffer0 = context.AddPass<ProxyProcessPass>(targets.GBuffer0);
		context.GBuffer1 = context.AddPass<ProxyProcessPass>(targets.GBuffer1);
		context.Depth = context.AddPass<ProxyProcessPass>(targets.DepthStencil);
		context.LastOutput = context.GBuffer0;

		if (CVarSSAOEnabled.Get())
		{
			AddSSAO(context);
		}

		context.ProcessAllPasses();

		context.LastOutput.GetOutput()->TargetRef->GetTexture().Bind(0);
		renderer->SetDefaultRenderTarget();
		renderer->ShaderControl->BindPipeline<BasicPostVS, ScreenCoverFS>(renderer->GlobalShaders);
		DrawScreenCover();
	}

	void PostProcessingContext::ProcessAllPasses()
	{
		for (auto pass : passes)
		{
			// Propagate dependencies.
			for (uint32 id = 0;; id++)
			{
				ProcessPassOutputRef *input = pass->GetInput(id);
				if (!input) break;

				input->GetOutput()->AddDependency();
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
			// Set up inputs.
			for (uint32 id = 0;; id++)
			{
				ProcessPassOutputRef *input = pass->GetInput(id);
				if (!input) break;

				input->GetOutput()->TargetRef->GetTexture().Bind(id);
			}

			//Debugf("Process %s", pass->Name());
			pass->Process(this);

			// Release dependencies.
			for (uint32 id = 0;; id++)
			{
				ProcessPassOutputRef *input = pass->GetInput(id);
				if (!input) break;

				input->GetOutput()->ReleaseDependency();
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
