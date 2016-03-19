#include "graphics/VulkanHelpers.hh"

namespace vk
{
	void APIVersion(uint32_t version, int &major, int &minor, int &patch)
	{
		major = version >> 22;
		minor = (version >> 12) & 0x3ff;
		patch = version & 0xfff;
	}

	void setImageLayout(vk::CommandBuffer cmdBuffer, vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout)
	{
		vk::ImageMemoryBarrier barrier;
		barrier.srcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
		barrier.dstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);

		barrier.oldLayout(oldImageLayout);
		barrier.newLayout(newImageLayout);
		barrier.image(image);

		barrier.subresourceRange().aspectMask(aspectMask);
		barrier.subresourceRange().baseMipLevel(0);
		barrier.subresourceRange().levelCount(1);
		barrier.subresourceRange().layerCount(1);

		// Source layouts

		//if (oldImageLayout == vk::ImageLayout::eUndefined)
		//	barrier.srcAccessMask(vk::AccessFlagBits::eHostWrite | vk::AccessFlagBits::eTransferWrite);

		if (oldImageLayout == vk::ImageLayout::eColorAttachmentOptimal)
			barrier.srcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

		if (oldImageLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
			barrier.srcAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);

		if (oldImageLayout == vk::ImageLayout::eTransferSrcOptimal)
			barrier.srcAccessMask(vk::AccessFlagBits::eTransferRead);

		if (oldImageLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
			barrier.srcAccessMask(vk::AccessFlagBits::eShaderRead);

		// Target layouts

		if (newImageLayout == vk::ImageLayout::eTransferDstOptimal)
			barrier.dstAccessMask(vk::AccessFlagBits::eTransferWrite);

		if (newImageLayout == vk::ImageLayout::eTransferSrcOptimal)
		{
			barrier.srcAccessMask() |= vk::AccessFlagBits::eTransferRead;
			barrier.dstAccessMask(vk::AccessFlagBits::eTransferRead);
		}

		if (newImageLayout == vk::ImageLayout::eColorAttachmentOptimal)
		{
			barrier.srcAccessMask(vk::AccessFlagBits::eTransferRead);
			barrier.dstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
		}

		if (newImageLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
			barrier.dstAccessMask() |= vk::AccessFlagBits::eDepthStencilAttachmentWrite;

		if (newImageLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
		{
			barrier.srcAccessMask(vk::AccessFlagBits::eHostWrite | vk::AccessFlagBits::eTransferWrite);
			barrier.dstAccessMask(vk::AccessFlagBits::eShaderRead);
		}

		auto stageMask = vk::PipelineStageFlagBits::eTopOfPipe;
		cmdBuffer.pipelineBarrier(stageMask, stageMask, vk::DependencyFlags(), {}, {}, { barrier });
	}
}
