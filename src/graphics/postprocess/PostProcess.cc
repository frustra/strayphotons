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

	void PostProcessing::Process(Renderer *renderer, const EngineRenderTargets &targets)
	{
		PostProcessingContext context;
		context.renderer = renderer;

		auto gbuffer0 = context.AddPass<ProxyProcessPass>(targets.GBuffer0);
		auto gbuffer1 = context.AddPass<ProxyProcessPass>(targets.GBuffer1);
		auto depth = context.AddPass<ProxyProcessPass>(targets.DepthStencil);

		context.LastOutput = ProcessPassOutputRef(gbuffer0);

		if (CVarSSAOEnabled.Get())
		{
			auto ssaoPass0 = context.AddPass<SSAOPass0>();
			ssaoPass0->SetInput(0, context.LastOutput);
			ssaoPass0->SetInput(1, { gbuffer1 });
			ssaoPass0->SetInput(2, { depth });

			if (CVarDebugSSAO.Get())
			{
				context.LastOutput = ProcessPassOutputRef(ssaoPass0);
			}
			else
			{
				auto ssaoBlurX = context.AddPass<SSAOBlur>(true);
				ssaoBlurX->SetInput(0, { ssaoPass0 });
				ssaoBlurX->SetInput(1, { gbuffer1 });
				ssaoBlurX->SetInput(2, { depth });
				ssaoBlurX->SetInput(3, { gbuffer0 });

				auto ssaoBlurY = context.AddPass<SSAOBlur>(false);
				ssaoBlurY->SetInput(0, { ssaoBlurX });
				ssaoBlurY->SetInput(1, { gbuffer1 });
				ssaoBlurY->SetInput(2, { depth });
				ssaoBlurY->SetInput(3, { gbuffer0 });

				context.LastOutput = ProcessPassOutputRef(ssaoBlurY);
			}
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
