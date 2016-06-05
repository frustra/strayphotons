#pragma once

#include "PostProcess.hh"
#include "core/Logging.hh"

namespace sp
{
	class SSAO : public PostProcessPass<1, 1>
	{
	public:
		void Process()
		{
			Debugf("yo");
			//SetRenderTarget(outputs[0].renderTarget->GetTexture());
			SetOutputTarget(0, GetInput(0).GetOutput()->renderTarget);
		}

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			return GetInput(id).GetOutput()->renderTargetDesc;
		}
	};
}