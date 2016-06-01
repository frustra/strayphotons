#include "graphics/postprocess/PostProcess.hh"

#include "graphics/postprocess/SSAO.hh"

namespace sp {
	ProcessPassOutput *ProcessPassOutputRef::GetOutput()
	{
		return pass->GetOutput(outputIndex);
	}

	void PostProcessing::Process()
	{
		//RenderTarget::Ref lastOutput;
		ProcessPassOutputRef lastOutput;
		SSAO ssao;
		ssao.SetInput(0, lastOutput);
	}
}