#include "graphics/GraphicsQueue.hh"
#include "graphics/Device.hh"

namespace sp
{
	GraphicsQueue::GraphicsQueue(Device &device, uint32 familyIndex)
		: device(device), familyIndex(familyIndex)
	{
		queue = device->getQueue(familyIndex, 0);
	}
}