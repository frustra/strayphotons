//=== Copyright Frustra Software, all rights reserved ===//

#include "graphics/DeviceAllocator.hh"
#include "graphics/Device.hh"
#include "core/Logging.hh"

namespace sp
{
	DeviceAllocator::~DeviceAllocator()
	{
	}

	void DeviceAllocator::SetDevice(vk::PhysicalDevice physical, vk::Device dev)
	{
		physicalDevice = physical;
		device = dev;
		memoryProps = physical.getMemoryProperties();
	}

	DeviceAllocation DeviceAllocator::Alloc(uint32 typeBits, vk::MemoryPropertyFlags props, uint64 size)
	{
		uint32 typeIndex = MemoryTypeIndex(typeBits, props);
		vk::MemoryAllocateInfo depthAllocInfo(size, typeIndex);

		// TODO(pushrax): suballocate from large blocks for performance

		DeviceAllocation allocation;
		allocation.mem = device.allocateMemory(depthAllocInfo, nalloc);
		allocation.offset = 0;
		allocation.size = size;
		allocation.allocator = this;
		return allocation;
	}

	DeviceAllocation DeviceAllocator::Alloc(vk::MemoryRequirements memReqs, vk::MemoryPropertyFlags props)
	{
		return Alloc(memReqs.memoryTypeBits(), props, memReqs.size());
	}

	DeviceAllocation DeviceAllocator::Alloc(vk::Buffer buf, vk::MemoryPropertyFlags props)
	{
		return Alloc(device.getBufferMemoryRequirements(buf), props);
	}

	DeviceAllocation DeviceAllocator::Alloc(vk::Image img, vk::MemoryPropertyFlags props)
	{
		return Alloc(device.getImageMemoryRequirements(img), props);
	}

	void DeviceAllocator::Free(DeviceAllocation &alloc)
	{
		device.freeMemory(alloc.mem, nalloc);
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
		return allocator->device.mapMemory(mem, offset, size, {});
	}

	void *DeviceAllocation::MapRange(uint64 start, uint64 len)
	{
		if (start + len > size)
			throw "DeviceAllocation::MapRange: out of bounds";

		return allocator->device.mapMemory(mem, offset + start, len, {});
	}

	void DeviceAllocation::Unmap()
	{
		allocator->device.unmapMemory(mem);
	}

	DeviceAllocation DeviceAllocation::BindBuffer(vk::Buffer buf)
	{
		allocator->device.bindBufferMemory(buf, mem, offset);
		return *this;
	}

	DeviceAllocation DeviceAllocation::BindImage(vk::Image image)
	{
		allocator->device.bindImageMemory(image, mem, offset);
		return *this;
	}
}