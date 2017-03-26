#pragma once

#include "PostProcess.hh"

namespace sp
{
	class ViewGBuffer : public PostProcessPass<7, 1>
	{
	public:
		ViewGBuffer(int mode, int source, int level, VoxelData voxelData) : mode(mode), source(source), level(level), voxelData(voxelData) {}
		void Process(const PostProcessingContext *context);

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			auto desc = GetInput(0)->GetOutput()->TargetDesc;
			desc.format = PF_SRGB8_A8;
			return desc;
		}

		string Name()
		{
			return "ViewGBuffer";
		}

	private:
		int mode, source, level;
		VoxelData voxelData;
	};
}
