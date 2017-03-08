#pragma once

#include "PostProcess.hh"

namespace sp
{
	class BloomHighpass : public PostProcessPass<1, 1>
	{
	public:
		void Process(const PostProcessingContext *context);

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			auto desc = GetInput(0)->GetOutput()->TargetDesc;
			desc.extent.x /= 2;
			desc.extent.y /= 2;
			return desc;
		}

		string Name()
		{
			return "BloomHighpass";
		}
	};

	class BloomBlur : public PostProcessPass<1, 1>
	{
	public:
		BloomBlur(glm::ivec2 direction, int downsample = 1, float clip = FLT_MAX, float scale = 1.0f)
			: direction(direction), downsample(downsample), clip(clip), scale(scale) { }

		void Process(const PostProcessingContext *context);

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			auto desc = GetInput(0)->GetOutput()->TargetDesc;
			desc.extent.x /= downsample;
			desc.extent.y /= downsample;
			return desc;
		}

		string Name()
		{
			return "BloomBlur";
		}

		glm::ivec2 direction;
		int downsample;
		float clip, scale;
	};

	class BloomCombine : public PostProcessPass<3, 1>
	{
	public:
		void Process(const PostProcessingContext *context);

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			return GetInput(0)->GetOutput()->TargetDesc;
		}

		string Name()
		{
			return "BloomCombine";
		}
	};
}