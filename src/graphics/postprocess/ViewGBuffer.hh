#pragma once

#include "PostProcess.hh"

namespace sp
{
	class ViewGBuffer : public PostProcessPass<6, 1>
	{
	public:
		ViewGBuffer(int mode, int level) : mode(mode), level(level) {}

		void Process(const PostProcessingContext *context);

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			auto desc = GetInput(0)->GetOutput()->TargetDesc;
			desc.format = PF_RGBA16F;
			return desc;
		}

		string Name()
		{
			return "ViewGBuffer";
		}

	private:
		int mode, level;
	};
}
