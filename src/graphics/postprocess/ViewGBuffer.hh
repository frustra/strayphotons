#pragma once

#include "PostProcess.hh"

namespace sp
{
	class ViewGBuffer : public PostProcessPass<4, 1>
	{
	public:
		ViewGBuffer(int mode) : mode(mode) {}

		void Process(const PostProcessingContext *context);

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			return GetInput(0)->GetOutput()->TargetDesc;
		}

		string Name()
		{
			return "ViewGBuffer";
		}

	private:
		int mode;
	};
}
