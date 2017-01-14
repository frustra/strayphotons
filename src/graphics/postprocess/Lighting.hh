#pragma once

#include "PostProcess.hh"

namespace sp
{
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

	class HDRTest : public PostProcessPass<1, 1>
	{
	public:
		void Process(const PostProcessingContext *context);

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			auto desc = GetInput(0)->GetOutput()->TargetDesc;
			//desc.format = PF_SRGB8_A8;
			return desc;
		}

		string Name()
		{
			return "HDRTest";
		}
	};
}