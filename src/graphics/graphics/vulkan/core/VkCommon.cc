#include "VkCommon.hh"

#include "core/Common.hh"
#include "core/Logging.hh"

namespace sp::vulkan {
    void AssertVKSuccess(vk::Result result, std::string message) {
        if (result == vk::Result::eSuccess) return;
        Abortf("%s (%s)", message, vk::to_string(result));
    }

    void AssertVKSuccess(VkResult result, std::string message) {
        AssertVKSuccess(static_cast<vk::Result>(result), message);
    }
} // namespace sp::vulkan
