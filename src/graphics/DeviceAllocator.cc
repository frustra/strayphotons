//=== Copyright Frustra Software, all rights reserved ===//

#include "graphics/DeviceAllocator.hh"
#include "core/Logging.hh"

namespace sp
{
	DeviceAllocator::DeviceAllocator(vk::Device &dev, vk::PhysicalDevice physicalDev, vk::AllocationCallbacks &hostAlloc)
		: vkdev(dev), hostAlloc(hostAlloc)
	{
		memoryProps = physicalDev.getMemoryProperties();
	}

	DeviceAllocator::~DeviceAllocator()
	{
	}

	DeviceAllocation DeviceAllocator::Alloc(uint32 typeBits, vk::MemoryPropertyFlags props, uint64 size)
	{
		uint32 typeIndex = MemoryTypeIndex(typeBits, props);
		vk::MemoryAllocateInfo depthAllocInfo(size, typeIndex);

		// TODO(pushrax): suballocate from large blocks for performance

		DeviceAllocation allocation;
		allocation.mem = vkdev.allocateMemory(depthAllocInfo, hostAlloc);
		allocation.offset = 0;
		allocation.size = size;
		allocation.allocator = this;
		return allocation;
	}

	DeviceAllocation DeviceAllocator::Alloc(vk::MemoryRequirements memReqs, vk::MemoryPropertyFlags props)
	{
		return Alloc(memReqs.memoryTypeBits(), props, memReqs.size());
	}

	void DeviceAllocator::Free(DeviceAllocation &alloc)
	{
		vkdev.freeMemory(alloc.mem, hostAlloc);
		alloc.mem = {};
	}

	/*
	 * Find index of memory type given possible indexes and required properties.
	 */
	uint32 DeviceAllocator::MemoryTypeIndex(uint32 typeBits, vk::MemoryPropertyFlags props)
	{
		auto types = memoryProps.memoryTypes();

		for (uint32 i = 0; i < memoryProps.memoryTypeCount(); i++)
		{
			if ((typeBits & 1) == 1)
			{
				// Type index is allowed

				if ((types[i].propertyFlags() & props) == props)
				{
					// Memory type has required properties
					return i;
				}
			}

			typeBits >>= 1;
		}

		throw vk::Exception({}, "DeviceAllocator::MemoryTypeIndex: could not locate required memory type");
	}

	void *DeviceAllocation::Map()
	{
		return allocator->vkdev.mapMemory(mem, offset, size, {});
	}

	void DeviceAllocation::Unmap()
	{
		allocator->vkdev.unmapMemory(mem);
	}

	void DeviceAllocation::BindBuffer(vk::Buffer buf)
	{
		allocator->vkdev.bindBufferMemory(buf, mem, offset);
	}

	void DeviceAllocation::BindImage(vk::Image image)
	{
		allocator->vkdev.bindImageMemory(image, mem, offset);
	}
}