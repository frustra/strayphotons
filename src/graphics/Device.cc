#include "graphics/Device.hh"
#include "graphics/GraphicsQueue.hh"
#include "core/Logging.hh"

namespace sp
{
	static vk::PhysicalDeviceFeatures applicationFeatures()
	{
		vk::PhysicalDeviceFeatures feat;

		feat.depthClamp(true);
		feat.samplerAnisotropy(true);
		feat.tessellationShader(true);
		feat.robustBufferAccess(true);
		feat.fullDrawIndexUint32(true);

		return feat;
	}

	Device::~Device()
	{
		Assert(!device, "Device::~Device: must call Destroy()");
	}

	void Device::Destroy()
	{
		if (!device) return;

		if (primaryQueue)
			delete primaryQueue;

		if (pipelineCache)
			device.destroyPipelineCache(pipelineCache, nullptr);

		device.destroy(nullptr);
		device = {};
	}

	void Device::Initialize(vk::PhysicalDevice physical)
	{
		Assert(!physicalDevice, "Device already initialized");
		physicalDevice = physical;

		vector<const char *> deviceExts =
		{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		};

		vector<const char *> layers;

#ifdef VULKAN_ENABLE_VALIDATION
		layers.push_back("VK_LAYER_LUNARG_standard_validation");
#endif

		// Find and configure queues
		auto queueFamilyProps = physicalDevice.getQueueFamilyProperties();

		int primaryQueueIndex = -1;

		for (size_t i = 0; i < queueFamilyProps.size(); i++)
		{
			auto props = queueFamilyProps[i];
			auto flags = props.queueFlags();

			// Vulkan sec.4.1: if graphics is supported, at least one family must support both
			if (flags & vk::QueueFlagBits::eGraphics && flags & vk::QueueFlagBits::eCompute)
			{
				// TODO(pushrax): find a queue that supports presentation
				primaryQueueIndex = (int)i;
			}

			auto flagStr = to_string(flags);
			Logf("Queue fam %d - %s (0x%x), count: %d, timestamp valid bits: %d", i, flagStr.c_str(), (uint32_t)flags, props.queueCount(), props.timestampValidBits());
		}

		if (primaryQueueIndex < 0)
		{
			// TODO(pushrax): maybe support separate presentation and graphics queues
			Errorf("No queue families support presentation, graphics, and compute");
			throw "no valid queues";
		}

		float maxPriority = 1.0f;
		vk::DeviceQueueCreateInfo queueInfo;
		queueInfo.queueFamilyIndex((unsigned) primaryQueueIndex);
		queueInfo.queueCount(1);
		queueInfo.pQueuePriorities(&maxPriority);

		vk::DeviceQueueCreateInfo queueInfos[] = { queueInfo };


		// Create device
		auto features = applicationFeatures();
		vk::DeviceCreateInfo devInfo;
		devInfo.queueCreateInfoCount(sizeof(queueInfos) / sizeof(*queueInfos));
		devInfo.pQueueCreateInfos(queueInfos);
		devInfo.pEnabledFeatures(&features);
		devInfo.enabledLayerCount(layers.size());
		devInfo.ppEnabledLayerNames(layers.data());
		devInfo.enabledExtensionCount(deviceExts.size());
		devInfo.ppEnabledExtensionNames(deviceExts.data());

		device = physicalDevice.createDevice(devInfo, nullptr);

		primaryQueue = new GraphicsQueue(*this, primaryQueueIndex);


		// Create pipeline cache
		vk::PipelineCacheCreateInfo pipelineCacheInfo;
		pipelineCache = device.createPipelineCache(pipelineCacheInfo, nullptr);

		memoryManager.SetDevice(physicalDevice, device);
	}
}