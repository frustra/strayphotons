#pragma once

#include "PostProcess.hh"

namespace sp
{
	class SSAOPass0 : public PostProcessPass<3, 1>
	{
	public:
		void Process(const PostProcessingContext *context);

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			return GetInput(0)->GetOutput()->TargetDesc;
		}

		string Name()
		{
			return "SSAOPass0";
		}
	};

	class SSAOBlur : public PostProcessPass<4, 1>
	{
	public:
		SSAOBlur(bool horizontal)
			: horizontal(horizontal) {}

		void Process(const PostProcessingContext *context);

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			return GetInput(0)->GetOutput()->TargetDesc;
		}

		string Name()
		{
			return "SSAOBlur";
		}

	private:
		bool horizontal;
	};
}
