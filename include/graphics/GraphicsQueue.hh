#ifndef SP_GRAPHICS_QUEUE_H
#define SP_GRAPHICS_QUEUE_H

#include "Shared.hh"
#include "graphics/Graphics.hh"

namespace sp
{
	class Device;

	class GraphicsQueue
	{
	public:
		GraphicsQueue(Device &device, uint32 familyIndex);

		inline vk::Queue Handle()
		{
			return queue;
		}

		inline uint32 FamilyIndex()
		{
			return familyIndex;
		}

	private:
		Device &device;
		uint32 familyIndex;
		vk::Queue queue;
	};
}

#endif