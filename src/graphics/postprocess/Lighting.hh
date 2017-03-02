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

	class LumiHistogram : public PostProcessPass<1, 1>
	{
	public:
		void Process(const PostProcessingContext *context);

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			return GetInput(0)->GetOutput()->TargetDesc;
		}

		string Name()
		{
			return "LumiHistogram";
		}
	};

	class VoxelLighting : public PostProcessPass<9, 1>
	{
	public:
		VoxelLighting(VoxelData voxelData, Buffer mirrorVisData, Buffer mirrorSceneData)
			: voxelData(voxelData), mirrorVisData(mirrorVisData), mirrorSceneData(mirrorSceneData) {}

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
	private:
		VoxelData voxelData;
		Buffer mirrorVisData;
		Buffer mirrorSceneData;
	};

	class VoxelLightingDiffuse : public PostProcessPass<5, 1>
	{
	public:
		VoxelLightingDiffuse(VoxelData voxelData);

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
		VoxelData voxelData;
		int downsample = 1;
	};
}