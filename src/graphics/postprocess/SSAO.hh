#pragma once

#include "PostProcess.hh"

namespace sp
{
	class SSAO : public PostProcessPass<3, 1>
	{
	public:
		void Process(const PostProcessingContext *context);

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			return GetInput(0)->GetOutput()->TargetDesc;
		}
	};
}
