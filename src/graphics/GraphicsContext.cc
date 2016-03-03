//=== Copyright Frustra Software, all rights reserved ===//

#include "core/Game.hh"
#include "core/Logging.hh"
#include "graphics/GraphicsContext.hh"

#include <string>
#include <iostream>

namespace sp
{
	static vk::PhysicalDeviceFeatures applicationFeatures() {
		vk::PhysicalDeviceFeatures feat;

		feat.depthClamp(true);
		feat.samplerAnisotropy(true);
		feat.tessellationShader(true);
		feat.robustBufferAccess(true);
		feat.fullDrawIndexUint32(true);

		return feat;
	}

	GraphicsContext::GraphicsContext() : window(NULL), monitor(NULL)
	{
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		//glfwWindowHint(GLFW_DEPTH_BITS, 24);
	}

	GraphicsContext::~GraphicsContext()
	{
		if (vksurface)
			vk::destroySurfaceKHR(vkinstance, vksurface, allocator);

		if (vkdevice)
			vk::destroyDevice(vkdevice, allocator);

		if (vkinstance)
			vk::destroyInstance(vkinstance, allocator);

		if (window)
			glfwDestroyWindow(window);
	}

	void GraphicsContext::CreateWindow()
	{
		int nLayers = 1;
		const char *layers[] =
		{
			"VK_LAYER_LUNARG_standard_validation",
		};

		int nExts;
		const char **extensions = glfwGetRequiredInstanceExtensions(&nExts);

		vk::ApplicationInfo appInfo("Stray Photons", 1, "SP ENGINE", 1, VK_API_VERSION);

		vk::InstanceCreateInfo info;
		info.pApplicationInfo(&appInfo);
		info.enabledLayerCount(nLayers);
		info.ppEnabledLayerNames(layers);
		info.enabledExtensionCount(nExts);
		info.ppEnabledExtensionNames(extensions);

		// Initialize Vulkan
		vk::Assert(vk::createInstance(&info, allocator, &vkinstance), "creating Vulkan instance");

		int major, minor, patch;
		vk::APIVersion(VK_API_VERSION, major, minor, patch);
		Logf("Created Vulkan instance with API v%d.%d.%d", major, minor, patch);

		auto devices = vkinstance.enumeratePhysicalDevices();

		if (devices.size() == 0) {
			Errorf("No Vulkan devices found");
			throw "no devices";
		}

		// Find compatible device
		vk::PhysicalDevice device;

		for (auto curr : devices)
		{
			auto props = curr.getProperties();
			vk::APIVersion(props.apiVersion(), major, minor, patch);
			Logf("Available Vulkan device: %s (%x:%x), v%d.%d.%d", props.deviceName(), props.vendorID(), props.deviceID(), major, minor, patch);

			if (device) continue;
			auto devType = props.deviceType();

			if (devType == vk::PhysicalDeviceType::eDiscreteGpu) {
				// TODO(pushrax): choose first integrated GPU if discrete is not present
				device = curr;
			}
		}

		auto devProps = device.getProperties();
		Logf("Using device %x:%x", devProps.vendorID(), devProps.deviceID());

		// Find and configure queues
		auto queueFamilyProps = device.getQueueFamilyProperties();

		int primaryQueueIndex = -1;

		for (size_t i = 0; i < queueFamilyProps.size(); i++) {
			auto props = queueFamilyProps[i];
			auto flags = props.queueFlags();

			// If an implementation exposes any queue family that supports graphics operations,
			// at least one queue family of at least one physical device exposed by the implementation
			// must support both graphics and compute operations. Vulkan sec.4.1
			if (flags & vk::QueueFlagBits::eGraphics && flags & vk::QueueFlagBits::eCompute) {
				primaryQueueIndex = (int) i;
			}

			Logf("Queue fam %d - flags: 0x%x, count: %d, timestamp valid bits: %d", i, flags, props.queueCount(), props.timestampValidBits());
		}

		if (primaryQueueIndex < 0) {
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
		devInfo.enabledLayerCount(nLayers);
		devInfo.ppEnabledLayerNames(layers);
		devInfo.pEnabledFeatures(&features);

		vk::Assert(vk::createDevice(device, &devInfo, allocator, &vkdevice), "creating device");

		// Create window and surface
		if (!(window = glfwCreateWindow(1440, 810, "Stray Photons", monitor, NULL)))
		{
			throw "glfw window creation failed";
		}

		auto result = glfwCreateWindowSurface((VkInstance)vkinstance, window, (VkAllocationCallbacks *)allocator, ((VkSurfaceKHR *)&vksurface));
		vk::Assert(result, "creating window surface");
	}

	bool GraphicsContext::ShouldClose()
	{
		return !!glfwWindowShouldClose(window);
	}

	void GraphicsContext::RenderFrame()
	{
		/* glfwSwapBuffers(window); */
	}
}

