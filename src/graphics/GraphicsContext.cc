//=== Copyright Frustra Software, all rights reserved ===//

#include "core/Game.hh"
#include "core/Logging.hh"
#include "graphics/GraphicsContext.hh"
#include "graphics/VulkanHelpers.hh"
#include "graphics/Shader.hh"
#include "graphics/Device.hh"

#include <string>
#include <iostream>

#ifdef VULKAN_ENABLE_VALIDATION
PFN_vkCreateDebugReportCallbackEXT fpCreateDebugReportCallback;
PFN_vkDestroyDebugReportCallbackEXT fpDestroyDebugReportCallback;
PFN_vkDebugReportMessageEXT fpDebugReportMessage;
#endif

namespace sp
{
#ifdef VULKAN_ENABLE_VALIDATION
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
#endif

	GraphicsContext::GraphicsContext()
	{
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		shaderSet = make_shared<ShaderSet>();
	}

	GraphicsContext::~GraphicsContext()
	{
		for (auto buf : framebuffers)
			device->destroyFramebuffer(buf, nalloc);

		if (renderPass)
			device->destroyRenderPass(renderPass, nalloc);

		if (depthStencil.view)
			device->destroyImageView(depthStencil.view, nalloc);

		if (depthStencil.image)
			device->destroyImage(depthStencil.image, nalloc);

		if (depthStencil.mem)
			device.Memory().Free(depthStencil.mem);

		if (prePresentCmdBuffer || postPresentCmdBuffer)
			device->freeCommandBuffers(cmdPool, { prePresentCmdBuffer, postPresentCmdBuffer });

		if (!drawCmdBuffers.empty())
			device->freeCommandBuffers(cmdPool, drawCmdBuffers);

		for (auto view : swapchainViews)
			device->destroyImageView(view, nalloc);

		if (vkswapchain)
			device->destroySwapchainKHR(vkswapchain, nalloc);

		if (cmdPool)
			device->destroyCommandPool(cmdPool, nalloc);

		if (device)
			device.Destroy();

		if (vksurface)
			vkinstance.destroySurfaceKHR(vksurface, nalloc);

#ifdef VULKAN_ENABLE_VALIDATION
		if (debugReportCallback)
			fpDestroyDebugReportCallback((VkInstance) vkinstance, debugReportCallback, (VkAllocationCallbacks *) &nalloc);
#endif

		if (vkinstance)
			vkinstance.destroy(nalloc);

		if (window)
			glfwDestroyWindow(window);
	}

	void GraphicsContext::CreateWindow()
	{
		int nGlfwExts;
		const char **glfwExts = glfwGetRequiredInstanceExtensions(&nGlfwExts);

		vector<const char *> instExts(glfwExts, glfwExts + nGlfwExts);

#ifdef VULKAN_ENABLE_VALIDATION
		instExts.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif

		vector<const char *> layers;

#ifdef VULKAN_ENABLE_VALIDATION
		layers.push_back("VK_LAYER_LUNARG_standard_validation");
#endif


		// Initialize Vulkan
		vk::ApplicationInfo appInfo("Stray Photons", 1, "SP ENGINE", 1, VK_API_VERSION);

		vk::InstanceCreateInfo info;
		info.pApplicationInfo(&appInfo);
		info.enabledLayerCount(layers.size());
		info.ppEnabledLayerNames(layers.data());
		info.enabledExtensionCount(instExts.size());
		info.ppEnabledExtensionNames(instExts.data());

		vk::Assert(vk::createInstance(&info, &nalloc, &vkinstance), "creating Vulkan instance");

		int major, minor, patch;
		vk::APIVersion(VK_API_VERSION, major, minor, patch);
		Logf("Created Vulkan instance with API v%d.%d.%d", major, minor, patch);

		auto devices = vkinstance.enumeratePhysicalDevices();

		if (devices.size() == 0)
		{
			Errorf("No Vulkan devices found");
			throw "no devices";
		}


#ifdef VULKAN_ENABLE_VALIDATION
		fpCreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT) vkinstance.getProcAddr("vkCreateDebugReportCallbackEXT");
		fpDestroyDebugReportCallback = (PFN_vkDestroyDebugReportCallbackEXT) vkinstance.getProcAddr("vkDestroyDebugReportCallbackEXT");
		fpDebugReportMessage = (PFN_vkDebugReportMessageEXT) vkinstance.getProcAddr("vkDebugReportMessageEXT");

		VkDebugReportCallbackCreateInfoEXT dbgInfo = {};
		dbgInfo.pNext = NULL;
		dbgInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
		dbgInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		dbgInfo.flags |= VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		//dbgInfo.flags |= VK_DEBUG_REPORT_INFORMATION_BIT_EXT;

		dbgInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)&vulkanCallback;
		auto err = fpCreateDebugReportCallback((VkInstance) vkinstance, &dbgInfo, (VkAllocationCallbacks *) &nalloc, &debugReportCallback);
		vk::Assert(err, "creating debug report callback");
#endif

		vk::PhysicalDevice physicalDevice;

		// Find compatible device
		for (auto curr : devices)
		{
			auto props = curr.getProperties();
			vk::APIVersion(props.apiVersion(), major, minor, patch);
			Logf("Available Vulkan device: %s (%x:%x), v%d.%d.%d", props.deviceName(), props.vendorID(), props.deviceID(), major, minor, patch);

			if (physicalDevice) continue;
			auto devType = props.deviceType();

			if (devType == vk::PhysicalDeviceType::eDiscreteGpu)
			{
				// TODO(pushrax): choose first integrated GPU if discrete is not present
				physicalDevice = curr;
			}
		}

		auto devProps = physicalDevice.getProperties();
		Logf("Using device %x:%x", devProps.vendorID(), devProps.deviceID());

		device.Initialize(physicalDevice);


		// Create window and surface
		if (!(window = glfwCreateWindow(1280, 720, "Stray Photons", monitor, NULL)))
		{
			throw "glfw window creation failed";
		}

		auto result = glfwCreateWindowSurface((VkInstance) vkinstance, window, (VkAllocationCallbacks *) &nalloc, (VkSurfaceKHR *) &vksurface);
		vk::Assert(result, "creating window surface");



		// Select surface format
		vector<vk::SurfaceFormatKHR> surfaceFormats;
		physicalDevice.getSurfaceFormatsKHR(vksurface, surfaceFormats);

		if (surfaceFormats.size() == 0)
		{
			throw "no surface formats";
		}

		// TODO(pushrax): select sRGB
		colorFormat = surfaceFormats[0].format();
		colorSpace = surfaceFormats[0].colorSpace();

		if (surfaceFormats.size() == 1 && surfaceFormats[0].format() == vk::Format::eUndefined)
		{
			colorFormat = vk::Format::eB8G8R8A8Unorm;
		}


		// Create command pool
		vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, 0 /* TODOASAP queue family index */);
		cmdPool = device->createCommandPool(poolInfo, nalloc);

		vk::CommandBufferAllocateInfo setupCmdInfo(cmdPool, vk::CommandBufferLevel::ePrimary, 1);
		auto buffers = device->allocateCommandBuffers(setupCmdInfo);
		setupCmdBuffer = buffers[0];
		setupCmdBuffer.begin({});


		// Initialize swapchain
		uint32 width = 1280, height = 720;
		ResetSwapchain(width, height);


		// Create command buffers
		vk::CommandBufferAllocateInfo drawCmdInfo(cmdPool, vk::CommandBufferLevel::ePrimary, swapchainImages.size());
		drawCmdBuffers = device->allocateCommandBuffers(drawCmdInfo);

		vk::CommandBufferAllocateInfo presentCmdInfo(cmdPool, vk::CommandBufferLevel::ePrimary, 2);
		buffers = device->allocateCommandBuffers(presentCmdInfo);
		prePresentCmdBuffer = buffers[0];
		postPresentCmdBuffer = buffers[1];


		// Create depth buffer (depth + stencil)
		vk::ImageCreateInfo depthInfo;
		depthInfo.imageType(vk::ImageType::e2D);
		depthInfo.format(depthFormat);
		depthInfo.extent({ width, height, 1 });
		depthInfo.mipLevels(1);
		depthInfo.arrayLayers(1);
		depthInfo.samples(vk::SampleCountFlagBits::e1);
		depthInfo.tiling(vk::ImageTiling::eOptimal);
		depthInfo.usage(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferSrc);

		depthStencil.image = device->createImage(depthInfo, nalloc);

		vk::ImageViewCreateInfo depthStencilInfo;
		depthStencilInfo.viewType(vk::ImageViewType::e2D);
		depthStencilInfo.format(depthFormat);
		depthStencilInfo.subresourceRange().aspectMask(vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
		depthStencilInfo.subresourceRange().baseMipLevel(0);
		depthStencilInfo.subresourceRange().levelCount(1);
		depthStencilInfo.subresourceRange().baseArrayLayer(0);
		depthStencilInfo.subresourceRange().layerCount(1);

		depthStencil.mem = device.Memory().AllocDeviceLocal(depthStencil.image);
		depthStencil.mem.BindImage(depthStencil.image);

		vk::setImageLayout(setupCmdBuffer, depthStencil.image, vk::ImageAspectFlagBits::eDepth, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
		depthStencilInfo.image(depthStencil.image);
		depthStencil.view = device->createImageView(depthStencilInfo, nalloc);


		// Create render pass
		vk::AttachmentDescription colorAttachment, depthAttachment;

		colorAttachment.format(colorFormat);
		colorAttachment.samples(vk::SampleCountFlagBits::e1);
		colorAttachment.loadOp(vk::AttachmentLoadOp::eClear);
		colorAttachment.storeOp(vk::AttachmentStoreOp::eStore);
		colorAttachment.stencilLoadOp(vk::AttachmentLoadOp::eDontCare);
		colorAttachment.stencilStoreOp(vk::AttachmentStoreOp::eDontCare);
		colorAttachment.initialLayout(vk::ImageLayout::eColorAttachmentOptimal);
		colorAttachment.finalLayout(vk::ImageLayout::eColorAttachmentOptimal);

		depthAttachment.format(depthFormat);
		depthAttachment.samples(vk::SampleCountFlagBits::e1);
		depthAttachment.loadOp(vk::AttachmentLoadOp::eClear);
		depthAttachment.storeOp(vk::AttachmentStoreOp::eStore);
		depthAttachment.stencilLoadOp(vk::AttachmentLoadOp::eDontCare);
		depthAttachment.stencilStoreOp(vk::AttachmentStoreOp::eDontCare);
		depthAttachment.initialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
		depthAttachment.finalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

		vk::AttachmentReference colorReference(0, vk::ImageLayout::eColorAttachmentOptimal);
		vk::AttachmentReference depthReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);

		vk::SubpassDescription subpass;
		subpass.pipelineBindPoint(vk::PipelineBindPoint::eGraphics);
		subpass.colorAttachmentCount(1);
		subpass.pColorAttachments(&colorReference);
		subpass.pDepthStencilAttachment(&depthReference);

		vk::AttachmentDescription attachmentDescs[] = { colorAttachment, depthAttachment };

		vk::RenderPassCreateInfo renderPassInfo;
		renderPassInfo.attachmentCount(2);
		renderPassInfo.pAttachments(attachmentDescs);
		renderPassInfo.subpassCount(1);
		renderPassInfo.pSubpasses(&subpass);

		renderPass = device->createRenderPass(renderPassInfo, nalloc);


		// Set up framebuffer
		vk::ImageView attachments[2];
		attachments[1] = depthStencil.view;

		vk::FramebufferCreateInfo framebufferInfo;
		framebufferInfo.renderPass(renderPass);
		framebufferInfo.attachmentCount(2);
		framebufferInfo.pAttachments(attachments);
		framebufferInfo.width(width);
		framebufferInfo.height(height);
		framebufferInfo.layers(1);

		for (size_t i = 0; i < swapchainImages.size(); i++)
		{
			attachments[0] = swapchainViews[i];
			framebuffers.push_back(device->createFramebuffer(framebufferInfo, nalloc));
		}


		// Submit setup queue
		setupCmdBuffer.end();

		vk::SubmitInfo submitInfo;
		submitInfo.commandBufferCount(1);
		submitInfo.pCommandBuffers(&setupCmdBuffer);
		device.PrimaryQueue().submit({ submitInfo }, vk::Fence());

		device.PrimaryQueue().waitIdle();
		device->freeCommandBuffers(cmdPool, { setupCmdBuffer });

		Prepare();
	}

	void GraphicsContext::ResetSwapchain(uint32 &width, uint32 &height)
	{
		auto oldSwapchain = vkswapchain;

		for (auto view : swapchainViews)
		{
			device->destroyImageView(view, nalloc);
		}

		vk::SurfaceCapabilitiesKHR surfCap;
		device.Physical().getSurfaceCapabilitiesKHR(vksurface, surfCap);

		vector<vk::PresentModeKHR> presentModes;
		device.Physical().getSurfacePresentModesKHR(vksurface, presentModes);

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
		createInfo.imageFormat(colorFormat);
		createInfo.imageColorSpace(colorSpace);
		createInfo.imageExtent(swapchainExtent);
		createInfo.imageUsage(vk::ImageUsageFlagBits::eColorAttachment);
		createInfo.preTransform(preTransform);
		createInfo.imageArrayLayers(1);
		createInfo.imageSharingMode(vk::SharingMode::eExclusive);
		createInfo.presentMode(presentMode);
		createInfo.oldSwapchain(oldSwapchain);
		createInfo.clipped(true);
		createInfo.compositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);

		vkswapchain = device->createSwapchainKHR(createInfo, nalloc);

		if (oldSwapchain)
		{
			device->destroySwapchainKHR(oldSwapchain, nalloc);
		}

		device->getSwapchainImagesKHR(vkswapchain, swapchainImages);

		swapchainViews.resize(swapchainImages.size());

		for (size_t i = 0; i < swapchainImages.size(); i++)
		{
			vk::ImageViewCreateInfo viewCreateInfo;
			viewCreateInfo.format(colorFormat);
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
			auto view = device->createImageView(viewCreateInfo, nalloc);
			swapchainViews[i] = view;
		}
	}

	bool GraphicsContext::ShouldClose()
	{
		return !!glfwWindowShouldClose(window);
	}
}
