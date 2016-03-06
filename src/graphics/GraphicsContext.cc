//=== Copyright Frustra Software, all rights reserved ===//

#include "core/Game.hh"
#include "core/Logging.hh"
#include "graphics/GraphicsContext.hh"
#include "graphics/VulkanHelpers.hh"

#include <string>
#include <iostream>

PFN_vkCreateDebugReportCallbackEXT fpCreateDebugReportCallback;
PFN_vkDestroyDebugReportCallbackEXT fpDestroyDebugReportCallback;
PFN_vkDebugReportMessageEXT fpDebugReportMessage;

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

	static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanCallback(
		VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT objectType,
		uint64_t object,
		size_t location,
		int32_t messageCode,
		const char *layerPrefix,
		const char *message,
		void *userData)
	{
		if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
			Errorf("Vk [%s] (%d): %s", layerPrefix, messageCode, message);
		else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
			Logf("Vk [%s] (%d): %s", layerPrefix, messageCode, message);
		else
			Debugf("Vk [%s] (%d): %s", layerPrefix, messageCode, message);
		return false;
	}

	GraphicsContext::GraphicsContext()
	{
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		//glfwWindowHint(GLFW_DEPTH_BITS, 24);
	}

	GraphicsContext::~GraphicsContext()
	{
		if (depthBuffer.view)
			vkdevice.destroyImageView(depthBuffer.view, alloc);

		if (depthBuffer.image)
			vkdevice.destroyImage(depthBuffer.image, alloc);

		if (depthBuffer.mem)
			vkdevice.freeMemory(depthBuffer.mem, alloc);

		for (auto view : swapchainViews)
			vkdevice.destroyImageView(view, alloc);

		if (vkswapchain)
			vkdevice.destroySwapchainKHR(vkswapchain, alloc);

		if (cmdPool)
			vkdevice.destroyCommandPool(cmdPool, alloc);

		if (vkdevice)
			vkdevice.destroy(alloc);

		if (vksurface)
			vkinstance.destroySurfaceKHR(vksurface, alloc);

		if (enableValidation && debugReportCallback)
			fpDestroyDebugReportCallback((VkInstance) vkinstance, debugReportCallback, (VkAllocationCallbacks *) &alloc);

		if (vkinstance)
			vkinstance.destroy(alloc);

		if (window)
			glfwDestroyWindow(window);
	}

	void GraphicsContext::CreateWindow()
	{
		int nGlfwExts;
		const char **glfwExts = glfwGetRequiredInstanceExtensions(&nGlfwExts);

		vector<const char *> layers =
		{
			"VK_LAYER_LUNARG_standard_validation",
		};

		vector<const char *> instExts(glfwExts, glfwExts + nGlfwExts);

		if (enableValidation)
			instExts.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

		vector<const char *> deviceExts =
		{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		};


		// Initialize Vulkan
		vk::ApplicationInfo appInfo("Stray Photons", 1, "SP ENGINE", 1, VK_API_VERSION);

		vk::InstanceCreateInfo info;
		info.pApplicationInfo(&appInfo);
		info.enabledLayerCount(layers.size());
		info.ppEnabledLayerNames(layers.data());
		info.enabledExtensionCount(instExts.size());
		info.ppEnabledExtensionNames(instExts.data());

		vk::Assert(vk::createInstance(&info, &alloc, &vkinstance), "creating Vulkan instance");

		int major, minor, patch;
		vk::APIVersion(VK_API_VERSION, major, minor, patch);
		Logf("Created Vulkan instance with API v%d.%d.%d", major, minor, patch);

		auto devices = vkinstance.enumeratePhysicalDevices();

		if (devices.size() == 0)
		{
			Errorf("No Vulkan devices found");
			throw "no devices";
		}


		// Set up validation/debugging
		if (enableValidation)
		{
			fpCreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT) vkinstance.getProcAddr("vkCreateDebugReportCallbackEXT");
			fpDestroyDebugReportCallback = (PFN_vkDestroyDebugReportCallbackEXT) vkinstance.getProcAddr("vkDestroyDebugReportCallbackEXT");
			fpDebugReportMessage = (PFN_vkDebugReportMessageEXT) vkinstance.getProcAddr("vkDebugReportMessageEXT");

			VkDebugReportCallbackCreateInfoEXT dbgInfo = {};
			dbgInfo.pNext = NULL;
			dbgInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
			dbgInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
			dbgInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)&vulkanCallback;
			auto err = fpCreateDebugReportCallback((VkInstance) vkinstance, &dbgInfo, (VkAllocationCallbacks *) &alloc, &debugReportCallback);
			vk::Assert(err, "creating debug report callback");
		}


		// Find compatible device
		for (auto curr : devices)
		{
			auto props = curr.getProperties();
			vk::APIVersion(props.apiVersion(), major, minor, patch);
			Logf("Available Vulkan device: %s (%x:%x), v%d.%d.%d", props.deviceName(), props.vendorID(), props.deviceID(), major, minor, patch);

			if (vkpdevice) continue;
			auto devType = props.deviceType();

			if (devType == vk::PhysicalDeviceType::eDiscreteGpu)
			{
				// TODO(pushrax): choose first integrated GPU if discrete is not present
				vkpdevice = curr;
			}
		}

		auto devProps = vkpdevice.getProperties();
		Logf("Using device %x:%x", devProps.vendorID(), devProps.deviceID());
		deviceMemoryProps = vkpdevice.getMemoryProperties();


		// Create window and surface
		if (!(window = glfwCreateWindow(720, 480, "Stray Photons", monitor, NULL)))
		{
			throw "glfw window creation failed";
		}

		auto result = glfwCreateWindowSurface((VkInstance)vkinstance, window, (VkAllocationCallbacks *) &alloc, (VkSurfaceKHR *) &vksurface);
		vk::Assert(result, "creating window surface");


		// Find and configure queues
		auto queueFamilyProps = vkpdevice.getQueueFamilyProperties();

		int primaryQueueIndex = -1;

		for (size_t i = 0; i < queueFamilyProps.size(); i++)
		{
			auto props = queueFamilyProps[i];
			auto flags = props.queueFlags();
			auto supportsPresent = vkpdevice.getSurfaceSupportKHR(i, vksurface);

			// Vulkan sec.4.1: if graphics is supported, at least one family must support both
			if (flags & vk::QueueFlagBits::eGraphics && flags & vk::QueueFlagBits::eCompute)
			{
				if (supportsPresent)
				{
					primaryQueueIndex = (int)i;
				}
			}

			Logf("Queue fam %d - flags: 0x%x, count: %d, timestamp valid bits: %d", i, flags, props.queueCount(), props.timestampValidBits());
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

		vkdevice = vkpdevice.createDevice(devInfo, alloc);
		auto queue = vkdevice.getQueue(primaryQueueIndex, 0);


		// Select surface format
		vector<vk::SurfaceFormatKHR> surfaceFormats;
		vkpdevice.getSurfaceFormatsKHR(vksurface, surfaceFormats);

		if (surfaceFormats.size() == 0)
		{
			throw "no surface formats";
		}

		// TODO(pushrax): select sRGB
		surfaceColorFormat = surfaceFormats[0].format();
		surfaceColorSpace = surfaceFormats[0].colorSpace();

		if (surfaceFormats.size() == 1 && surfaceFormats[0].format() == vk::Format::eUndefined)
		{
			surfaceColorFormat = vk::Format::eB8G8R8A8Unorm;
		}


		// Create command pool
		vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, primaryQueueIndex);
		cmdPool = vkdevice.createCommandPool(poolInfo, alloc);

		vk::CommandBufferAllocateInfo setupCmdInfo(cmdPool, vk::CommandBufferLevel::ePrimary, 1);
		auto buffers = vkdevice.allocateCommandBuffers(setupCmdInfo);
		setupCmdBuffer = buffers[0];
		setupCmdBuffer.begin({});


		// Initialize swapchain
		uint32 width = 720, height = 480;
		ResetSwapchain(width, height);


		// Create command buffers
		vk::CommandBufferAllocateInfo drawCmdInfo(cmdPool, vk::CommandBufferLevel::ePrimary, swapchainImages.size());
		drawCmdBuffers = vkdevice.allocateCommandBuffers(setupCmdInfo);

		vk::CommandBufferAllocateInfo presentCmdInfo(cmdPool, vk::CommandBufferLevel::ePrimary, 2);
		buffers = vkdevice.allocateCommandBuffers(presentCmdInfo);
		prePresentCmdBuffer = buffers[0];
		postPresentCmdBuffer = buffers[1];


		// Create depth buffer (depth + stencil)
		vk::ImageCreateInfo depthInfo;
		depthInfo.imageType(vk::ImageType::e2D);
		depthInfo.format(vk::Format::eD24UnormS8Uint);
		depthInfo.extent({ width, height, 1 });
		depthInfo.mipLevels(1);
		depthInfo.arrayLayers(1);
		depthInfo.samples(vk::SampleCountFlagBits::e1);
		depthInfo.tiling(vk::ImageTiling::eOptimal);
		depthInfo.usage(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferSrc);

		depthBuffer.image = vkdevice.createImage(depthInfo, alloc);
		auto depthMemReqs = vkdevice.getImageMemoryRequirements(depthBuffer.image);

		vk::ImageViewCreateInfo depthStencilInfo;
		depthStencilInfo.viewType(vk::ImageViewType::e2D);
		depthStencilInfo.format(vk::Format::eD24UnormS8Uint);
		depthStencilInfo.subresourceRange().aspectMask(vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
		depthStencilInfo.subresourceRange().baseMipLevel(0);
		depthStencilInfo.subresourceRange().levelCount(1);
		depthStencilInfo.subresourceRange().baseArrayLayer(0);
		depthStencilInfo.subresourceRange().layerCount(1);

		vk::MemoryAllocateInfo depthAllocInfo(depthMemReqs.size(), 0);
		getMemoryType(depthMemReqs.memoryTypeBits(), vk::MemoryPropertyFlagBits::eDeviceLocal, depthAllocInfo.memoryTypeIndex());
		depthBuffer.mem = vkdevice.allocateMemory(depthAllocInfo, alloc);
		vkdevice.bindImageMemory(depthBuffer.image, depthBuffer.mem, 0);
		vk::setImageLayout(setupCmdBuffer, depthBuffer.image, vk::ImageAspectFlagBits::eDepth, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
		depthStencilInfo.image(depthBuffer.image);
		depthBuffer.view = vkdevice.createImageView(depthStencilInfo, alloc);


		// Submit setup queue
		setupCmdBuffer.end();

		vk::SubmitInfo submitInfo;
		submitInfo.commandBufferCount(1);
		submitInfo.pCommandBuffers(&setupCmdBuffer);
		queue.submit({ submitInfo }, vk::Fence());

		queue.waitIdle();
		vkdevice.freeCommandBuffers(cmdPool, { setupCmdBuffer });
	}

	/*
	 * Find index of memory type given valid indexes and required properties.
	 */
	bool GraphicsContext::getMemoryType(uint32 typeBits, vk::MemoryPropertyFlags properties, uint32 &typeIndex)
	{
		auto types = deviceMemoryProps.memoryTypes();

		for (uint32 i = 0; i < deviceMemoryProps.memoryTypeCount(); i++)
		{
			if ((typeBits & 1) == 1)
			{
				if ((types[i].propertyFlags() & properties) == properties)
				{
					typeIndex = i;
					return true;
				}
			}
			typeBits >>= 1;
		}
		return false;
	}

	void GraphicsContext::ResetSwapchain(uint32 &width, uint32 &height)
	{
		auto oldSwapchain = vkswapchain;

		for (auto view : swapchainViews)
		{
			vkdevice.destroyImageView(view, alloc);
		}

		vk::SurfaceCapabilitiesKHR surfCap;
		vkpdevice.getSurfaceCapabilitiesKHR(vksurface, surfCap);

		vector<vk::PresentModeKHR> presentModes;
		vkpdevice.getSurfacePresentModesKHR(vksurface, presentModes);

		vk::Extent2D swapchainExtent;
		if (surfCap.currentExtent().width() == -1)
		{
			// width/height are both -1
			swapchainExtent.width(width);
			swapchainExtent.height(height);
		}
		else
		{
			swapchainExtent = surfCap.currentExtent();
			width = surfCap.currentExtent().width();
			height = surfCap.currentExtent().height();
		}

		// Prefer MAILBOX > IMMEDIATE > FIFO
		vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifoKHR;
		for (auto mode : presentModes)
		{
			if (mode == vk::PresentModeKHR::eMailboxKHR)
			{
				presentMode = mode;
				break;
			}

			if (presentMode == vk::PresentModeKHR::eFifoKHR && mode == vk::PresentModeKHR::eImmediateKHR)
			{
				presentMode = mode;
			}
		}

		uint32 desiredImageCount = surfCap.minImageCount() + 1;
		if (surfCap.maxImageCount() > 0 && desiredImageCount > surfCap.maxImageCount())
		{
			desiredImageCount = surfCap.maxImageCount();
		}

		auto preTransform = surfCap.currentTransform();
		if (surfCap.supportedTransforms() & vk::SurfaceTransformFlagBitsKHR::eIdentity)
		{
			preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
		}

		vk::SwapchainCreateInfoKHR createInfo;
		createInfo.surface(vksurface);
		createInfo.minImageCount(desiredImageCount);
		createInfo.imageFormat(surfaceColorFormat);
		createInfo.imageColorSpace(surfaceColorSpace);
		createInfo.imageExtent(swapchainExtent);
		createInfo.imageUsage(vk::ImageUsageFlagBits::eColorAttachment);
		createInfo.preTransform(preTransform);
		createInfo.imageArrayLayers(1);
		createInfo.imageSharingMode(vk::SharingMode::eExclusive);
		createInfo.presentMode(presentMode);
		createInfo.oldSwapchain(oldSwapchain);
		createInfo.clipped(true);
		createInfo.compositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);

		vkswapchain = vkdevice.createSwapchainKHR(createInfo, alloc);

		if (oldSwapchain)
		{
			vkdevice.destroySwapchainKHR(oldSwapchain, alloc);
		}

		vkdevice.getSwapchainImagesKHR(vkswapchain, swapchainImages);

		swapchainViews.resize(swapchainImages.size());

		for (size_t i = 0; i < swapchainImages.size(); i++)
		{
			vk::ImageViewCreateInfo viewCreateInfo;
			viewCreateInfo.format(surfaceColorFormat);
			viewCreateInfo.components(vk::ComponentMapping(vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA));
			viewCreateInfo.viewType(vk::ImageViewType::e2D);

			vk::ImageSubresourceRange &range = viewCreateInfo.subresourceRange();
			range.aspectMask(vk::ImageAspectFlagBits::eColor);
			range.baseMipLevel(0);
			range.levelCount(1);
			range.baseArrayLayer(0);
			range.layerCount(1);

			auto image = swapchainImages[i];
			setImageLayout(setupCmdBuffer, image, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);

			viewCreateInfo.image(image);
			auto view = vkdevice.createImageView(viewCreateInfo, alloc);
			swapchainViews[i] = view;
		}
	}

	void GraphicsContext::InitDepthBuffer()
	{

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

