#pragma once

#include "PostProcess.hh"

namespace sp {
    class Tonemap : public PostProcessPass<1, 1> {
    public:
        void Process(PostProcessLock lock, const PostProcessingContext *context);

        RenderTargetDesc GetOutputDesc(uint32 id) {
            auto desc = GetInput(0)->GetOutput()->TargetDesc;
            desc.format = PF_RGBA8;
            return desc;
        }

        string Name() {
            return "Tonemap";
        }
    };

    class LumiHistogram : public PostProcessPass<1, 1> {
    public:
        void Process(PostProcessLock lock, const PostProcessingContext *context);

        RenderTargetDesc GetOutputDesc(uint32 id) {
            return GetInput(0)->GetOutput()->TargetDesc;
        }

        string Name() {
            return "LumiHistogram";
        }
    };

    class VoxelLighting : public PostProcessPass<12, 1> {
    public:
        VoxelLighting(VoxelContext voxelContext, bool ssaoEnabled)
            : voxelContext(voxelContext), ssaoEnabled(ssaoEnabled) {}

        void Process(PostProcessLock lock, const PostProcessingContext *context);

        RenderTargetDesc GetOutputDesc(uint32 id) {
            auto desc = GetInput(0)->GetOutput()->TargetDesc;
            desc.format = PF_RGBA16F;
            return desc;
        }

        string Name() {
            return "VoxelLighting";
        }

    private:
        VoxelContext voxelContext;
        bool ssaoEnabled;
    };

    class VoxelLightingDiffuse : public PostProcessPass<5, 1> {
    public:
        VoxelLightingDiffuse(VoxelContext voxelContext);

        void Process(PostProcessLock lock, const PostProcessingContext *context);

        RenderTargetDesc GetOutputDesc(uint32 id) {
            auto desc = GetInput(0)->GetOutput()->TargetDesc;
            desc.extent[0] /= downsample;
            desc.extent[1] /= downsample;
            desc.format = PF_RGBA16F;
            return desc;
        }

        string Name() {
            return "VoxelLightingDiffuse";
        }

    private:
        VoxelContext voxelContext;
        int downsample = 1;
    };
} // namespace sp
