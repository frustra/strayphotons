#ifndef SP_VULKAN_HELPERS_H
#define SP_VULKAN_HELPERS_H

#include <string>

#include "graphics/Graphics.hh"
#include "core/Logging.hh"

namespace vk
{
	static inline void Assert(vk::Result result, const char *msg)
	{
		if (result == vk::Result::eSuccess) return;

		auto str = to_string(result);
		Errorf("VkResult %s (%d) %s", str.c_str(), result, msg);
		throw msg;
	}

	static inline void Assert(VkResult result, const char *msg)
	{
		Assert(vk::Result(result), msg);
	}

	void APIVersion(uint32_t version, int &major, int &minor, int &patch);

	void setImageLayout(vk::CommandBuffer cmdBuffer, vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);
}

#endif