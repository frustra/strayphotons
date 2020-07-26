#pragma once

#include "PostProcess.hh"

namespace sp {
	class ProxyProcessPass : public PostProcessPass<0, 1> {
	public:
		ProxyProcessPass(RenderTarget::Ref input) : input(input) {
			Assert(input != nullptr, "null proxy pass input");
		}

		void Process(const PostProcessingContext *context) {
			SetOutputTarget(0, input);
		}

		RenderTargetDesc GetOutputDesc(uint32 id) {
			return input->GetDesc();
		}

		string Name() {
			return "ProxyTarget";
		}

	private:
		RenderTarget::Ref input;
	};
} // namespace sp