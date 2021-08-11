#pragma once

#include "PostProcess.hh"

namespace sp {
    class ViewGBuffer : public PostProcessPass<6, 1> {
    public:
        ViewGBuffer(int mode, int source, int level, VoxelContext voxelContext)
            : mode(mode), source(source), level(level), voxelContext(voxelContext) {}
        void Process(PostProcessLock lock, const PostProcessingContext *context);

        RenderTargetDesc GetOutputDesc(uint32 id) {
            auto desc = GetInput(0)->GetOutput()->TargetDesc;
            desc.format = PF_RGBA8;
            return desc;
        }

        string Name() {
            return "ViewGBuffer";
        }

    private:
        int mode, source, level;
        VoxelContext voxelContext;
    };
} // namespace sp
