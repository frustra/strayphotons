#pragma once

#include "PostProcess.hh"

namespace sp
{
	class VoxelLighting : public PostProcessPass<9, 1>
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
			return "VoxelLighting";
		}
	};

	class VoxelLightingDiffuse : public PostProcessPass<7, 1>
	{
	public:
		VoxelLightingDiffuse();

		void Process(const PostProcessingContext *context);

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			auto desc = GetInput(0)->GetOutput()->TargetDesc;
			desc.extent[0] /= downsample;
			desc.extent[1] /= downsample;
			desc.format = PF_RGBA16F;
			return desc;
		}

		string Name()
		{
			return "VoxelLightingDiffuse";
		}

	private:
		int downsample = 1;
	};
}
