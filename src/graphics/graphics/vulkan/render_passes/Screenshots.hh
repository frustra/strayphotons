#pragma once

#include "Common.hh"
#include "console/CFunc.hh"
#include "core/LockFreeMutex.hh"

namespace sp::vulkan::renderer {
    class Screenshots {
    public:
        Screenshots();
        void AddPass(RenderGraph &graph);

    private:
        CFuncCollection funcs;
        LockFreeMutex screenshotMutex;
        vector<std::pair<string, string>> pendingScreenshots;
    };

    void WriteScreenshot(DeviceContext &device, const std::string &path, const ImageViewPtr &view);
} // namespace sp::vulkan::renderer
