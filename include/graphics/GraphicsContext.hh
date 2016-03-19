#ifndef SP_GRAPHICSCONTEXT_H
#define SP_GRAPHICSCONTEXT_H

#include <string>

#include "graphics/Graphics.hh"
#include "graphics/Device.hh"
#include "Shared.hh"

namespace sp
{
	class Device;
	class ShaderSet;

	class GraphicsContext
	{
	public:
		GraphicsContext();
		virtual ~GraphicsContext();

		void CreateWindow();
		bool ShouldClose();
		void ResetSwapchain(uint32 &width, uint32 &height);

		virtual void Prepare() = 0;
		virtual void RenderFrame() = 0;

		shared_ptr<ShaderSet> shaderSet;
		Device device;

	private:
		GLFWwindow *window = nullptr;
		GLFWmonitor *monitor = nullptr;

#ifdef VULKAN_ENABLE_VALIDATION
		VkDebugReportCallbackEXT debugReportCallback = 0;
#endif

	protected:
		vk::Instance vkinstance;

		vk::CommandPool cmdPool;
		vk::CommandBuffer setupCmdBuffer, prePresentCmdBuffer, postPresentCmdBuffer;
		vector<vk::CommandBuffer> drawCmdBuffers;
		vk::RenderPass renderPass;

		vk::SurfaceKHR vksurface;
		vk::SwapchainKHR vkswapchain;
		vector<vk::Framebuffer> framebuffers;

		vk::Format colorFormat, depthFormat = vk::Format::eD24UnormS8Uint;
		vk::ColorSpaceKHR colorSpace;

		struct
		{
			vk::Image image;
			DeviceAllocation mem;
			vk::ImageView view;
		} depthStencil;

		// TODO(pushrax) another class
		vector<vk::Image> swapchainImages;
		vector<vk::ImageView> swapchainViews;
	};
}

#endif

