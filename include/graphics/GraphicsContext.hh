//=== Copyright Frustra Software, all rights reserved ===//

#ifndef SP_GRAPHICSCONTEXT_H
#define SP_GRAPHICSCONTEXT_H

#include <string>

#include "graphics/Graphics.hh"

namespace sp
{
	class GraphicsContext
	{
	public:
		GraphicsContext();
		virtual ~GraphicsContext();

		void CreateWindow();
		bool ShouldClose();
		void ResetSwapchain(uint32 &width, uint32 &height);
		bool GetMemoryType(uint32 typeBits, vk::MemoryPropertyFlags properties, uint32 &typeIndex);

		virtual void Prepare() = 0;
		virtual void RenderFrame() = 0;

		vk::ShaderModule CreateShaderModule(std::string filename, vk::ShaderStageFlagBits stage);
		vk::PipelineShaderStageCreateInfo LoadShader(std::string filename, vk::ShaderStageFlagBits stage);

	private:
		GLFWwindow *window = NULL;
		GLFWmonitor *monitor = NULL;

		bool enableValidation = true;

		VkDebugReportCallbackEXT debugReportCallback = 0;
		vector<vk::ShaderModule> shaderModules;

	protected:
		vk::Instance vkinstance;
		vk::PhysicalDevice vkpdevice;
		vk::Device vkdev;
		vk::PhysicalDeviceMemoryProperties deviceMemoryProps;
		vk::AllocationCallbacks &alloc = vk::AllocationCallbacks::null();

		vk::Queue vkqueue;
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
		} depthStencil;

		// TODO(pushrax) another class
		vector<vk::Image> swapchainImages;
		vector<vk::ImageView> swapchainViews;
	};
}

#endif

