#pragma once

#include "PostProcess.hh"

namespace sp
{
	class SMAAEdgeDetection : public PostProcessPass<1, 1>
	{
	public:
		void Process(const PostProcessingContext *context);

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			auto desc = GetInput(0)->GetOutput()->TargetDesc;
			desc.format = PF_RGBA8;
			return desc;
		}

		string Name()
		{
			return "SMAAEdgeDetection";
		}
	};

	class SMAABlendingWeights : public PostProcessPass<1, 1>
	{
	public:
		void Process(const PostProcessingContext *context);

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			// RGBA8 from edge detection.
			return GetInput(0)->GetOutput()->TargetDesc;
		}

		string Name()
		{
			return "SMAABlendingWeights";
		}
	};

	class SMAABlending : public PostProcessPass<2, 1>
	{
	public:
		void Process(const PostProcessingContext *context);

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			return GetInput(0)->GetOutput()->TargetDesc;
		}

		string Name()
		{
			return "SMAABlending";
		}
	};
}