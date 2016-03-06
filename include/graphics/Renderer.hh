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
			vk::DeviceMemory mem;
			vector<vk::VertexInputBindingDescription> bindingDescs;
			vector<vk::VertexInputAttributeDescription> attribDescs;
			vk::PipelineVertexInputStateCreateInfo inputInfo;
		} vertices;

		struct
		{
			vk::Buffer buf;
			vk::DeviceMemory mem;
			uint32 count;
		} indices;

		struct
		{
			vk::Buffer buf;
			vk::DeviceMemory mem;
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
		vk::Semaphore presentCompleteSem;

		uint32 currentBuffer = 0;
	};
}

#endif