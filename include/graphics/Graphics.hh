#ifndef SP_GRAPHICS_H
#define SP_GRAPHICS_H

#define GLFW_INCLUDE_VULKAN
#define VKCPP_ENHANCED_MODE

#include <GLFW/glfw3.h>
#include "vulkan/vk_cpp.h"

#include "Shared.hh"
#include "core/Logging.hh"

namespace vk
{
	static void Assert(vk::Result result, const char *msg)
	{
		if (result == vk::Result::eSuccess) return;

		auto str = vk::getString(result);
		Errorf("VkResult %s (%d) %s", str.c_str(), result, msg);
		throw msg;
	}

	static void Assert(VkResult result, const char *msg)
	{
		Assert(vk::Result(result), msg);
	}

	static void APIVersion(uint32_t version, int &major, int &minor, int &patch)
	{
		major = version >> 22;
		minor = (version >> 12) & 0x3ff;
		patch = version & 0xfff;
	}
}

#endif