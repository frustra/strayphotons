#pragma once

#include "PostProcess.hh"

namespace sp {
    class SSAOPass0 : public PostProcessPass<3, 1> {
    public:
        void Process(PostProcessLock lock, const PostProcessingContext *context);

        RenderTargetDesc GetOutputDesc(uint32 id) {
            auto desc = GetInput(0)->GetOutput()->TargetDesc;
            desc.format = PF_R16F;
            desc.extent.x /= 2;
            desc.extent.y /= 2;
            return desc;
        }

        string Name() {
            return "SSAOPass0";
        }
    };

    class SSAOBlur : public PostProcessPass<2, 1> {
    public:
        SSAOBlur(bool horizontal) : horizontal(horizontal) {}

        void Process(PostProcessLock lock, const PostProcessingContext *context);

        RenderTargetDesc GetOutputDesc(uint32 id) {
            auto desc = GetInput(0)->GetOutput()->TargetDesc;
            if (horizontal) {
                desc.extent.x *= 2;
                desc.extent.y *= 2;
            }
            return desc;
        }

        string Name() {
            return "SSAOBlur";
        }

    private:
        bool horizontal;
    };
} // namespace sp
