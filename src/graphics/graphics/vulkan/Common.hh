#pragma once

#include "core/Common.hh"

#include <string>
#include <vulkan/vulkan.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace sp::vulkan {
    typedef uint32 ShaderHandle;

    class CommandContext;
    typedef shared_ptr<CommandContext> CommandContextPtr;

    void AssertVKSuccess(vk::Result result, std::string message);
    void AssertVKSuccess(VkResult result, std::string message);

    template<typename T>
    class WrappedUniqueHandle : public NonCopyable {
    public:
        T Get() const {
            return *uniqueHandle;
        }

        T operator*() const {
            return *uniqueHandle;
        }

        T *operator->() {
            T &t = *uniqueHandle;
            return &t;
        }

    protected:
        vk::UniqueHandle<T, vk::DispatchLoaderDynamic> uniqueHandle;
    };
} // namespace sp::vulkan
