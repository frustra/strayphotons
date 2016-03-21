/*
 * DeviceAllocator is used to manage Vulkan memory allocations,
 * and suballocate from large device memory blocks.
 */

#ifndef SP_DEVICE_ALLOCATOR_H
#define SP_DEVICE_ALLOCATOR_H

#include "Common.hh"
#include "graphics/Graphics.hh"

namespace sp
{
	class Device;
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
		~DeviceAllocator();

		DeviceAllocation Alloc(uint32 typeBits, vk::MemoryPropertyFlags props, uint64 size);
		DeviceAllocation Alloc(vk::MemoryRequirements memReqs, vk::MemoryPropertyFlags props);
		DeviceAllocation Alloc(vk::Buffer buf, vk::MemoryPropertyFlags props);
		DeviceAllocation Alloc(vk::Image img, vk::MemoryPropertyFlags props);

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

		void Free(DeviceAllocation &alloc);

		uint32 MemoryTypeIndex(uint32 typeBits, vk::MemoryPropertyFlags props);
		void SetDevice(vk::PhysicalDevice physical, vk::Device dev);

	private:
		friend class DeviceAllocation;

		vk::Device device;
		vk::PhysicalDevice physicalDevice;
		vk::PhysicalDeviceMemoryProperties memoryProps;
	};
}

#endif