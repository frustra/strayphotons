#pragma once

#include "PostProcess.hh"

namespace sp {
    class GammaCorrect : public PostProcessPass<1, 1> {
    public:
        void Process(PostProcessLock lock, const PostProcessingContext *context);

        RenderTargetDesc GetOutputDesc(uint32 id) {
            auto desc = GetInput(0)->GetOutput()->TargetDesc;
            desc.format = PF_RGBA8;
            return desc;
        }

        string Name() {
            return "GammaCorrect";
        }
    };
} // namespace sp
