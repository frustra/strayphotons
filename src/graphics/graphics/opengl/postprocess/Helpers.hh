#pragma once

#include "PostProcess.hh"

#include <memory>

namespace sp {
    class ProxyProcessPass : public PostProcessPass<0, 1> {
    public:
        ProxyProcessPass(std::shared_ptr<GLRenderTarget> input) : input(input) {
            Assert(input != nullptr, "null proxy pass input");
        }

        void Process(PostProcessLock lock, const PostProcessingContext *context) {
            SetOutputTarget(0, input);
        }

        RenderTargetDesc GetOutputDesc(uint32 id) {
            return input->GetDesc();
        }

        string Name() {
            return "ProxyTarget";
        }

    private:
        std::shared_ptr<GLRenderTarget> input;
    };
} // namespace sp
