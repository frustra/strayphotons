#pragma once

#include "PostProcess.hh"

namespace sp
{
	class DeferredLighting : public PostProcessPass<3, 1>
	{
	public:
		void Process(const PostProcessingContext *context);

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			auto desc = GetInput(0)->GetOutput()->TargetDesc;
			desc.format = PF_RGBA16F;
			return desc;
		}

		string Name()
		{
			return "DeferredLighting";
		}
	};
}