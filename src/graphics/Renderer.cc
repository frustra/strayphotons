//=== Copyright Frustra Software, all rights reserved ===//

#include "graphics/Renderer.hh"
#include "core/Logging.hh"

#include <glm/gtc/matrix_transform.hpp>

namespace sp
{
	Renderer::~Renderer()
	{
		if (presentCompleteSem)
			vkdev.destroySemaphore(presentCompleteSem, alloc);

		if (descriptorPool)
			vkdev.destroyDescriptorPool(descriptorPool, alloc);

		if (pipeline)
			vkdev.destroyPipeline(pipeline, alloc);

		if (pipelineLayout)
			vkdev.destroyPipelineLayout(pipelineLayout, alloc);

		if (descriptorSetLayout)
			vkdev.destroyDescriptorSetLayout(descriptorSetLayout, alloc);

		if (vertices.buf)
			vkdev.destroyBuffer(vertices.buf, alloc);

		if (vertices.mem)
			vkdev.freeMemory(vertices.mem, alloc);

		if (indices.buf)
			vkdev.destroyBuffer(indices.buf, alloc);

		if (indices.mem)
			vkdev.freeMemory(indices.mem, alloc);

		if (uniformDataVS.buf)
			vkdev.destroyBuffer(uniformDataVS.buf, alloc);

		if (uniformDataVS.mem)
			vkdev.freeMemory(uniformDataVS.mem, alloc);
	}

	void Renderer::Prepare()
	{
		struct Vertex
		{
			float pos[3], color[3];
		};

		Vertex vertexBuf[] =
		{
			{ { 1, 1, 0 }, { 1, 0, 0 }},
			{ { -1, 1, 0 }, { 0, 1, 0 }},
			{ { 0, -1, 0 }, { 0, 0, 1 }},
		};

		uint32 indexBuf[] = { 0, 1, 2 };

		// Create vertex buffer
		vk::BufferCreateInfo vertexBufInfo;
		vertexBufInfo.size(sizeof(vertexBuf));
		vertexBufInfo.usage(vk::BufferUsageFlagBits::eVertexBuffer);

		vertices.buf = vkdev.createBuffer(vertexBufInfo, alloc);
		auto vertexMemReq = vkdev.getBufferMemoryRequirements(vertices.buf);

		vk::MemoryAllocateInfo vertexMemInfo(vertexMemReq.size(), 0);
		GetMemoryType(vertexMemReq.memoryTypeBits(), vk::MemoryPropertyFlagBits::eHostVisible, vertexMemInfo.memoryTypeIndex());
		vertices.mem = vkdev.allocateMemory(vertexMemInfo, alloc);

		// Upload to VRAM
		void *data = vkdev.mapMemory(vertices.mem, 0, vertexMemInfo.allocationSize(), vk::MemoryMapFlags());
		memcpy(data, vertexBuf, sizeof(vertexBuf));
		vkdev.unmapMemory(vertices.mem);

		vkdev.bindBufferMemory(vertices.buf, vertices.mem, 0);

		// Create index buffer
		vk::BufferCreateInfo indexBufInfo;
		indexBufInfo.size(sizeof(indexBuf));
		indexBufInfo.usage(vk::BufferUsageFlagBits::eIndexBuffer);

		indices.buf = vkdev.createBuffer(indexBufInfo, alloc);
		auto indexMemReq = vkdev.getBufferMemoryRequirements(indices.buf);

		vk::MemoryAllocateInfo indexMemInfo(indexMemReq.size(), 0);
		GetMemoryType(indexMemReq.memoryTypeBits(), vk::MemoryPropertyFlagBits::eHostVisible, indexMemInfo.memoryTypeIndex());
		indices.mem = vkdev.allocateMemory(indexMemInfo, alloc);

		// Upload to VRAM
		data = vkdev.mapMemory(indices.mem, 0, indexMemInfo.allocationSize(), vk::MemoryMapFlags());
		memcpy(data, indexBuf, sizeof(indexBuf));
		vkdev.unmapMemory(indices.mem);

		vkdev.bindBufferMemory(indices.buf, indices.mem, 0);
		indices.count = sizeof(indices) / sizeof(uint32);

		// Vertex binding description
		vertices.bindingDescs.resize(1);
		vertices.bindingDescs[0].binding(0);
		vertices.bindingDescs[0].stride(sizeof(Vertex));
		vertices.bindingDescs[0].inputRate(vk::VertexInputRate::eVertex);

		// Vertex attribute descriptions (position, color)
		vertices.attribDescs.resize(2);
		vertices.attribDescs[0].binding(0);
		vertices.attribDescs[0].location(0);
		vertices.attribDescs[0].format(vk::Format::eR32G32B32Sfloat);
		vertices.attribDescs[0].offset(0);

		vertices.attribDescs[1].binding(0);
		vertices.attribDescs[1].location(1);
		vertices.attribDescs[1].format(vk::Format::eR32G32B32Sfloat);
		vertices.attribDescs[1].offset(sizeof(float) * 3);

		// Assign descriptions
		vertices.inputInfo.vertexBindingDescriptionCount(vertices.bindingDescs.size());
		vertices.inputInfo.pVertexBindingDescriptions(vertices.bindingDescs.data());
		vertices.inputInfo.vertexAttributeDescriptionCount(vertices.attribDescs.size());
		vertices.inputInfo.pVertexAttributeDescriptions(vertices.attribDescs.data());


		// Prepare uniform buffers
		vk::BufferCreateInfo uboInfo;
		uboInfo.size(sizeof(uboVS));
		uboInfo.usage(vk::BufferUsageFlagBits::eUniformBuffer);

		uniformDataVS.buf = vkdev.createBuffer(uboInfo, alloc);
		auto uboMemReq = vkdev.getBufferMemoryRequirements(uniformDataVS.buf);

		vk::MemoryAllocateInfo uboMemInfo(uboMemReq.size(), 0);
		GetMemoryType(uboMemReq.memoryTypeBits(), vk::MemoryPropertyFlagBits::eHostVisible, uboMemInfo.memoryTypeIndex());
		uniformDataVS.mem = vkdev.allocateMemory(uboMemInfo, alloc);

		vkdev.bindBufferMemory(uniformDataVS.buf, uniformDataVS.mem, 0);

		uniformDataVS.desc.buffer(uniformDataVS.buf);
		uniformDataVS.desc.offset(0);
		uniformDataVS.desc.range(sizeof(uboVS));


		// Update uniforms
		uboVS.projection = glm::perspective(1.05f, 1.778f, 0.1f, 256.0f);
		uboVS.view = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, -2.5f));
		uboVS.model = glm::mat4();

		data = vkdev.mapMemory(uniformDataVS.mem, 0, sizeof(uboVS), vk::MemoryMapFlags());
		memcpy(data, &uboVS, sizeof(uboVS));
		vkdev.unmapMemory(uniformDataVS.mem);


		// Set descriptor layout
		vk::DescriptorSetLayoutBinding layoutBinding;
		layoutBinding.binding(0);
		layoutBinding.descriptorType(vk::DescriptorType::eUniformBuffer);
		layoutBinding.descriptorCount(1);
		layoutBinding.stageFlags(vk::ShaderStageFlagBits::eVertex);

		vk::DescriptorSetLayoutCreateInfo descriptorLayoutInfo;
		descriptorLayoutInfo.bindingCount(1);
		descriptorLayoutInfo.pBindings(&layoutBinding);

		descriptorSetLayout = vkdev.createDescriptorSetLayout(descriptorLayoutInfo, alloc);

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
		pipelineLayoutInfo.setLayoutCount(1);
		pipelineLayoutInfo.pSetLayouts(&descriptorSetLayout);

		pipelineLayout = vkdev.createPipelineLayout(pipelineLayoutInfo, alloc);


		// Prepare pipeline state
		vk::GraphicsPipelineCreateInfo pipelineInfo;
		pipelineInfo.layout(pipelineLayout);
		pipelineInfo.renderPass(renderPass);

		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState;
		inputAssemblyState.topology(vk::PrimitiveTopology::eTriangleList);

		vk::PipelineRasterizationStateCreateInfo rasterizationState;
		rasterizationState.cullMode(vk::CullModeFlagBits::eNone);

		vk::PipelineColorBlendAttachmentState blendAttachmentState[1];
		blendAttachmentState[0].colorWriteMask(vk::ColorComponentFlagBits(0xf));
		blendAttachmentState[0].blendEnable(false);

		vk::PipelineColorBlendStateCreateInfo colorBlendState;
		colorBlendState.attachmentCount(1);
		colorBlendState.pAttachments(blendAttachmentState);

		vk::PipelineViewportStateCreateInfo viewportState;
		viewportState.viewportCount(1);
		viewportState.scissorCount(1);

		vector<vk::DynamicState> dynamicStateEnables =
		{
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor,
		};

		vk::PipelineDynamicStateCreateInfo dynamicState;
		dynamicState.pDynamicStates(dynamicStateEnables.data());
		dynamicState.dynamicStateCount(dynamicStateEnables.size());

		vk::PipelineDepthStencilStateCreateInfo depthStencilState;
		depthStencilState.depthTestEnable(true);
		depthStencilState.depthWriteEnable(true);
		depthStencilState.depthCompareOp(vk::CompareOp::eLessOrEqual);
		depthStencilState.depthBoundsTestEnable(false);
		depthStencilState.back().failOp(vk::StencilOp::eKeep);
		depthStencilState.back().passOp(vk::StencilOp::eKeep);
		depthStencilState.back().compareOp(vk::CompareOp::eAlways);
		depthStencilState.stencilTestEnable(false);
		depthStencilState.front(depthStencilState.back());

		vk::PipelineMultisampleStateCreateInfo multisampleState;

		vk::PipelineShaderStageCreateInfo shaderStages[] =
		{
			LoadShader("../assets/shaders/triangle.vert.spv", vk::ShaderStageFlagBits::eVertex),
			LoadShader("../assets/shaders/triangle.frag.spv", vk::ShaderStageFlagBits::eFragment),
		};

		pipelineInfo.stageCount(2);
		pipelineInfo.pVertexInputState(&vertices.inputInfo);
		pipelineInfo.pInputAssemblyState(&inputAssemblyState);
		pipelineInfo.pRasterizationState(&rasterizationState);
		pipelineInfo.pColorBlendState(&colorBlendState);
		pipelineInfo.pMultisampleState(&multisampleState);
		pipelineInfo.pViewportState(&viewportState);
		pipelineInfo.pDepthStencilState(&depthStencilState);
		pipelineInfo.pStages(shaderStages);
		pipelineInfo.renderPass(renderPass);
		pipelineInfo.pDynamicState(&dynamicState);

		auto pipelines = vkdev.createGraphicsPipelines(pipelineCache, { pipelineInfo }, alloc);
		pipeline = pipelines[0];


		// Create descriptor pool
		vk::DescriptorPoolSize descriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1);

		vk::DescriptorPoolCreateInfo descriptorPoolInfo;
		descriptorPoolInfo.poolSizeCount(1);
		descriptorPoolInfo.pPoolSizes(&descriptorPoolSize);
		descriptorPoolInfo.maxSets(1);

		descriptorPool = vkdev.createDescriptorPool(descriptorPoolInfo, alloc);

		// Update descriptor sets with binding points
		vk::DescriptorSetAllocateInfo descSetInfo;
		descSetInfo.descriptorPool(descriptorPool);
		descSetInfo.descriptorSetCount(1);
		descSetInfo.pSetLayouts(&descriptorSetLayout);

		auto descriptorSets = vkdev.allocateDescriptorSets(descSetInfo);
		descriptorSet = descriptorSets[0];

		// Uniform buffer at binding 0
		vk::WriteDescriptorSet writeDescSet;
		writeDescSet.dstSet(descriptorSet);
		writeDescSet.descriptorCount(1);
		writeDescSet.descriptorType(vk::DescriptorType::eUniformBuffer);
		writeDescSet.pBufferInfo(&uniformDataVS.desc);
		writeDescSet.dstBinding(0);

		vkdev.updateDescriptorSets({ writeDescSet }, {});


		// Build command buffers, finally
		vk::CommandBufferBeginInfo cmdBufInfo;

		std::array<float, 4> clearColor = { 0.0f, 1.0f, 0.0f, 1.0f };

		vk::ClearValue clearValues[] =
		{
			vk::ClearValue(clearColor),
			vk::ClearValue({1.0f, 0}),
		};

		vk::RenderPassBeginInfo passBeginInfo;
		passBeginInfo.renderPass(renderPass);
		passBeginInfo.renderArea().offset().x(0);
		passBeginInfo.renderArea().offset().y(0);
		passBeginInfo.renderArea().extent().width(1280);
		passBeginInfo.renderArea().extent().height(720);
		passBeginInfo.clearValueCount(2);
		passBeginInfo.pClearValues(clearValues);

		for (size_t i = 0; i < drawCmdBuffers.size(); i++)
		{
			passBeginInfo.framebuffer(framebuffers[i]);

			auto cmdbuf = drawCmdBuffers[i];
			cmdbuf.begin(cmdBufInfo);
			cmdbuf.beginRenderPass(passBeginInfo, vk::SubpassContents::eInline);

			vk::Viewport viewport(0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f);
			cmdbuf.setViewport(0, { viewport });

			vk::Rect2D scissor({ 0, 0 }, { 1280, 720 });
			cmdbuf.setScissor(0, { scissor });

			cmdbuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, { descriptorSet }, {});
			cmdbuf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
			cmdbuf.bindVertexBuffers(0, { vertices.buf }, { {0} });
			cmdbuf.bindIndexBuffer(indices.buf, 0, vk::IndexType::eUint32);
			cmdbuf.drawIndexed(indices.count, 1, 0, 0, 1);

			cmdbuf.endRenderPass();

			vk::ImageMemoryBarrier prePresentBarrier;
			prePresentBarrier.srcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			prePresentBarrier.dstAccessMask(vk::AccessFlags());
			prePresentBarrier.oldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			prePresentBarrier.newLayout(vk::ImageLayout::ePresentSrcKHR);
			prePresentBarrier.subresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			prePresentBarrier.image(swapchainImages[i]);

			cmdbuf.pipelineBarrier(
				vk::PipelineStageFlagBits::eAllCommands,
				vk::PipelineStageFlagBits::eTopOfPipe,
				vk::DependencyFlags(), {}, {}, { prePresentBarrier }
			);

			cmdbuf.end();
		}

		vk::SemaphoreCreateInfo presentSemInfo;
		presentCompleteSem = vkdev.createSemaphore(presentSemInfo, alloc);
	}

	void Renderer::RenderFrame()
	{
		vkdev.waitIdle();
		vkdev.acquireNextImageKHR(vkswapchain, UINT64_MAX, presentCompleteSem, {}, currentBuffer);

		vk::SubmitInfo submitInfo;
		submitInfo.waitSemaphoreCount(1);
		submitInfo.pWaitSemaphores(&presentCompleteSem);
		submitInfo.commandBufferCount(1);
		submitInfo.pCommandBuffers(&drawCmdBuffers[currentBuffer]);
		vkqueue.submit({ submitInfo }, {});

		vk::PresentInfoKHR presentInfo;
		presentInfo.swapchainCount(1);
		presentInfo.pSwapchains(&vkswapchain);
		presentInfo.pImageIndices(&currentBuffer);
		vkqueue.presentKHR(presentInfo);

		vk::ImageMemoryBarrier postPresentBarrier;
		postPresentBarrier.srcAccessMask(vk::AccessFlags());
		postPresentBarrier.dstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
		postPresentBarrier.oldLayout(vk::ImageLayout::ePresentSrcKHR);
		postPresentBarrier.newLayout(vk::ImageLayout::eColorAttachmentOptimal);
		postPresentBarrier.subresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
		postPresentBarrier.image(swapchainImages[currentBuffer]);

		postPresentCmdBuffer.begin({});
		postPresentCmdBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eAllCommands,
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::DependencyFlags(), {}, {}, { postPresentBarrier }
		);
		postPresentCmdBuffer.end();

		vk::SubmitInfo postSubmitInfo;
		postSubmitInfo.commandBufferCount(1);
		postSubmitInfo.pCommandBuffers(&postPresentCmdBuffer);
		vkqueue.submit({ postSubmitInfo }, {});

		vkdev.waitIdle();
	}
}
