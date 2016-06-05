#pragma once

#include "PostProcess.hh"

namespace sp
{
	class ProxyProcessPass : public PostProcessPass<0, 1>
	{
	public:
		ProxyProcessPass(RenderTarget::Ref input)
			: input(input)
		{
			Assert(input != nullptr);
		}

		void Process()
		{
			Debugf("yo");
			SetOutputTarget(0, input);
		}

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			return input->GetDesc();
		}

	private:
		RenderTarget::Ref input;
	};
}