//=== Copyright Frustra Software, all rights reserved ===//

#ifndef SP_GRAPHICSCONTEXT_H
#define SP_GRAPHICSCONTEXT_H

#include "graphics/Graphics.hh"

namespace sp
{
	class GraphicsContext
	{
	public:
		GraphicsContext();
		~GraphicsContext();

		void CreateWindow();
		bool ShouldClose();
		void RenderFrame();
		void ResetSwapchain(uint32 &width, uint32 &height);
		bool getMemoryType(uint32 typeBits, vk::MemoryPropertyFlags properties, uint32 &typeIndex);

		//Viewport view;

	private:
		GLFWwindow *window = NULL;
		GLFWmonitor *monitor = NULL;

		bool enableValidation = true;

		vk::Instance vkinstance;
		vk::PhysicalDevice vkpdevice;
		vk::Device vkdevice;
		vk::PhysicalDeviceMemoryProperties deviceMemoryProps;
		vk::AllocationCallbacks &alloc = vk::AllocationCallbacks::null();
		VkDebugReportCallbackEXT debugReportCallback = 0;

		vk::CommandPool cmdPool;
		vk::CommandBuffer setupCmdBuffer, prePresentCmdBuffer, postPresentCmdBuffer;
		vector<vk::CommandBuffer> drawCmdBuffers;
		vk::RenderPass renderPass;
		vk::PipelineCache pipelineCache;

		vk::SurfaceKHR vksurface;
		vk::SwapchainKHR vkswapchain;
		vector<vk::Framebuffer> framebuffers;

		vk::Format colorFormat, depthFormat = vk::Format::eD24UnormS8Uint;
		vk::ColorSpaceKHR colorSpace;

		struct
		{
			vk::Image image;
			vk::DeviceMemory mem;
			vk::ImageView view;
		} depthBuffer;

		// TODO(pushrax) another class
		vector<vk::Image> swapchainImages;
		vector<vk::ImageView> swapchainViews;
	};
}

#endif

