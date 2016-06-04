#include "graphics/postprocess/PostProcess.hh"
#include "graphics/postprocess/Helpers.hh"

#include "graphics/postprocess/SSAO.hh"

namespace sp
{
	ProcessPassOutput *ProcessPassOutputRef::GetOutput()
	{
		return pass->GetOutput(outputIndex);
	}

	void PostProcessing::Process(const EngineRenderTargets &targets)
	{
		PostProcessingContext context;

		ProxyProcessPass gbuffer0(targets.GBuffer0);
		context.LastOutput = ProcessPassOutputRef(&gbuffer0);

		SSAO ssao;
		ssao.SetInput(0, context.LastOutput);
	}
}