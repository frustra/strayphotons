#pragma once

#include "PostProcess.hh"

namespace sp {
    class Crosshair : public PostProcessPass<1, 1> {
    public:
        void Process(const PostProcessingContext *context);

        RenderTargetDesc GetOutputDesc(uint32 id) {
            return GetInput(0)->GetOutput()->TargetDesc;
        }

        string Name() {
            return "Crosshair";
        }
    };
} // namespace sp