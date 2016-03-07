//=== Copyright Frustra Software, all rights reserved ===//

#ifndef SP_RENDERER_H
#define SP_RENDERER_H

#include "graphics/GraphicsContext.hh"

namespace sp
{
	class Renderer : public GraphicsContext
	{
	public:
		~Renderer();

		void Prepare();
		void RenderFrame();

	private:
		struct
		{
			vk::Buffer buf;
			DeviceAllocation mem;
			vector<vk::VertexInputBindingDescription> bindingDescs;
			vector<vk::VertexInputAttributeDescription> attribDescs;
			vk::PipelineVertexInputStateCreateInfo inputInfo;
		} vertices;

		struct
		{
			vk::Buffer buf;
			DeviceAllocation mem;
			uint32 count;
		} indices;

		struct
		{
			vk::Buffer buf;
			DeviceAllocation mem;
			vk::DescriptorBufferInfo desc;
		} uniformDataVS;

		struct
		{
			glm::mat4 projection;
			glm::mat4 model;
			glm::mat4 view;
		} uboVS;

		vk::Pipeline pipeline;
		vk::PipelineLayout pipelineLayout;
		vk::DescriptorSet descriptorSet;
		vk::DescriptorSetLayout descriptorSetLayout;
		vk::DescriptorPool descriptorPool;

		struct
		{
			// Ensures last image is presented before the next frame is submitted
			vk::Semaphore presentComplete;

			// Ensures current frame is submitted before image is presented
			vk::Semaphore renderComplete;
		} semaphores;

		uint32 currentBuffer = 0;
	};
}

#endif