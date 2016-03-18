//=== Copyright Frustra Software, all rights reserved ===//

/*
 * DeviceAllocator is used to manage Vulkan memory allocations,
 * and suballocate from large device memory blocks.
 */

#ifndef SP_DEVICE_ALLOCATOR_H
#define SP_DEVICE_ALLOCATOR_H

#include "Shared.hh"
#include "graphics/Graphics.hh"

namespace sp
{
	class DeviceAllocator;

	class DeviceAllocation
	{
	public:
		vk::DeviceMemory mem;
		uint64 offset;
		uint64 size;

		void *Map();
		void *MapRange(uint64 start, uint64 len);
		void Unmap();
		DeviceAllocation BindBuffer(vk::Buffer buf);
		DeviceAllocation BindImage(vk::Image image);

		explicit operator bool() const
		{
			return !!mem;
		}
		bool operator!() const
		{
			return !mem;
		}

	private:
		friend class DeviceAllocator;
		DeviceAllocator *allocator;
	};

	class DeviceAllocator
	{
	public:
		DeviceAllocator(vk::Device &dev, vk::PhysicalDevice physicalDev, vk::AllocationCallbacks &hostAlloc);
		~DeviceAllocator();

		DeviceAllocation Alloc(uint32 typeBits, vk::MemoryPropertyFlags props, uint64 size);
		DeviceAllocation Alloc(vk::MemoryRequirements memReqs, vk::MemoryPropertyFlags props);

		void Free(DeviceAllocation &alloc);

		DeviceAllocation Alloc(vk::Buffer buf, vk::MemoryPropertyFlags props)
		{
			return Alloc(vkdev.getBufferMemoryRequirements(buf), props);
		}

		DeviceAllocation Alloc(vk::Image img, vk::MemoryPropertyFlags props)
		{
			return Alloc(vkdev.getImageMemoryRequirements(img), props);
		}

		template <typename T>
		DeviceAllocation AllocHostVisible(T description)
		{
			return Alloc(description, vk::MemoryPropertyFlagBits::eHostVisible);
		}

		template <typename T>
		DeviceAllocation AllocDeviceLocal(T description)
		{
			return Alloc(description, vk::MemoryPropertyFlagBits::eDeviceLocal);
		}

		uint32 MemoryTypeIndex(uint32 typeBits, vk::MemoryPropertyFlags props);

	private:
		friend class DeviceAllocation;

		vk::Device &vkdev;
		vk::PhysicalDeviceMemoryProperties memoryProps;
		vk::AllocationCallbacks &hostAlloc;
	};
}

#endif