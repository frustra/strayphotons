#include "graphics/postprocess/PostProcess.hh"
#include "graphics/postprocess/Helpers.hh"

#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/RenderTargetPool.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Util.hh"

#include "graphics/postprocess/SSAO.hh"

namespace sp
{
	ProcessPassOutput *ProcessPassOutputRef::GetOutput()
	{
		return pass->GetOutput(outputIndex);
	}

	void PostProcessing::Process(Renderer *renderer, const EngineRenderTargets &targets)
	{
		PostProcessingContext context;
		context.renderer = renderer;

		ProxyProcessPass gbuffer0(targets.GBuffer0);
		ProxyProcessPass gbuffer1(targets.GBuffer1);
		ProxyProcessPass depth(targets.DepthStencil);
		context.AddPass(&gbuffer0);
		context.AddPass(&gbuffer1);
		context.AddPass(&depth);

		context.LastOutput = ProcessPassOutputRef(&gbuffer0);

		SSAO ssao;
		ssao.SetInput(0, context.LastOutput);
		ssao.SetInput(1, ProcessPassOutputRef(&gbuffer1));
		ssao.SetInput(2, ProcessPassOutputRef(&depth));
		context.AddPass(&ssao);
		context.LastOutput = ProcessPassOutputRef(&ssao, 0);

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
		}
		return TargetRef;
	}
}
