#pragma once

#include "PostProcess.hh"

namespace sp
{
	class DeferredLighting : public PostProcessPass<4, 1>
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

	class Tonemap : public PostProcessPass<1, 1>
	{
	public:
		void Process(const PostProcessingContext *context);

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			auto desc = GetInput(0)->GetOutput()->TargetDesc;
			desc.format = PF_SRGB8_A8;
			return desc;
		}

		string Name()
		{
			return "Tonemap";
		}
	};
}