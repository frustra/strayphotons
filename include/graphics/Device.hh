//=== Copyright Frustra Software, all rights reserved ===//

#ifndef SP_DEVICE_H
#define SP_DEVICE_H

#include <string>

#include "graphics/Graphics.hh"
#include "graphics/DeviceAllocator.hh"

namespace sp
{
	class Device
	{
	public:
		~Device();

		void Initialize(vk::PhysicalDevice physicalDevice);
		void Destroy();

		inline vk::Device Instance()
		{
			return device;
		}

		inline vk::PhysicalDevice Physical()
		{
			return physicalDevice;
		}

		inline DeviceAllocator &Memory()
		{
			return memoryManager;
		}

		inline vk::Queue PrimaryQueue()
		{
			return primaryQueue;
		}

		inline vk::PipelineCache PipelineCache()
		{
			return pipelineCache;
		}

		inline vk::Device *operator->()
		{
			return &device;
		}

		explicit operator bool() const
		{
			return !!device;
		}

		bool operator!() const
		{
			return !device;
		}

	private:
		vk::PhysicalDevice physicalDevice;
		vk::Device device;

		DeviceAllocator memoryManager;

		vk::Queue primaryQueue;
		vk::PipelineCache pipelineCache;
	};
}

#endif

