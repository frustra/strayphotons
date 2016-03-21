#include "graphics/Renderer.hh"
#include "graphics/Shader.hh"
#include "graphics/GraphicsQueue.hh"
#include "core/Logging.hh"

namespace sp
{
	class TriangleVS : public Shader
	{
		SHADER_TYPE(TriangleVS)

		TriangleVS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
			Bind(transform, "ubo");
		}

		struct
		{
			glm::mat4 projection;
			glm::mat4 model;
			glm::mat4 view;
		} transform;
	};

	class TriangleFS : public Shader
	{
		SHADER_TYPE(TriangleFS)
		using Shader::Shader;
	};

	IMPLEMENT_SHADER_TYPE(TriangleVS, "triangle.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(TriangleFS, "triangle.frag", Fragment);

	Renderer::~Renderer()
	{
		if (semaphores.presentComplete)
			device->destroySemaphore(semaphores.presentComplete, nalloc);

		if (semaphores.renderComplete)
			device->destroySemaphore(semaphores.renderComplete, nalloc);

		if (descriptorPool)
			device->destroyDescriptorPool(descriptorPool, nalloc);

		if (pipeline)
			device->destroyPipeline(pipeline, nalloc);

		if (pipelineLayout)
			device->destroyPipelineLayout(pipelineLayout, nalloc);

		if (descriptorSetLayout)
			device->destroyDescriptorSetLayout(descriptorSetLayout, nalloc);

		if (vertices.buf)
			device->destroyBuffer(vertices.buf, nalloc);

		if (vertices.mem)
			device.Memory().Free(vertices.mem);

		if (indices.buf)
			device->destroyBuffer(indices.buf, nalloc);

		if (indices.mem)
			device.Memory().Free(indices.mem);

		if (shaderManager)
			delete shaderManager;
	}

	void Renderer::Prepare()
	{
		shaderManager = new ShaderManager(device);
		shaderManager->CompileAll(*shaderSet);

		auto triangleVS = shaderSet->Get<TriangleVS>();
		triangleVS->transform.projection = glm::perspective(glm::radians(60.0f), 1.778f, 0.1f, 256.0f);
		triangleVS->transform.view = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, -2.5f));
		triangleVS->transform.model = glm::mat4();

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

		vertices.buf = device->createBuffer(vertexBufInfo, nalloc);
		vertices.mem = device.Memory().AllocHostVisible(vertices.buf).BindBuffer(vertices.buf);

		// Upload to VRAM
		void *data = vertices.mem.Map();
		memcpy(data, vertexBuf, sizeof(vertexBuf));
		vertices.mem.Unmap();

		// Create index buffer
		vk::BufferCreateInfo indexBufInfo;
		indexBufInfo.size(sizeof(indexBuf));
		indexBufInfo.usage(vk::BufferUsageFlagBits::eIndexBuffer);

		indices.buf = device->createBuffer(indexBufInfo, nalloc);
		indices.mem = device.Memory().AllocHostVisible(indices.buf).BindBuffer(indices.buf);

		// Upload to VRAM
		data = indices.mem.Map();
		memcpy(data, indexBuf, sizeof(indexBuf));
		indices.mem.Unmap();
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


		// Set descriptor layout
		vk::DescriptorSetLayoutBinding layoutBinding;
		layoutBinding.binding(0);
		layoutBinding.descriptorType(vk::DescriptorType::eUniformBuffer);
		layoutBinding.descriptorCount(1);
		layoutBinding.stageFlags(vk::ShaderStageFlagBits::eVertex);

		vk::DescriptorSetLayoutCreateInfo descriptorLayoutInfo;
		descriptorLayoutInfo.bindingCount(1);
		descriptorLayoutInfo.pBindings(&layoutBinding);

		descriptorSetLayout = device->createDescriptorSetLayout(descriptorLayoutInfo, nalloc);

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
		pipelineLayoutInfo.setLayoutCount(1);
		pipelineLayoutInfo.pSetLayouts(&descriptorSetLayout);

		pipelineLayout = device->createPipelineLayout(pipelineLayoutInfo, nalloc);


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
			shaderSet->Get<TriangleVS>()->StageCreateInfo(),
			shaderSet->Get<TriangleFS>()->StageCreateInfo(),
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

		auto pipelines = device->createGraphicsPipelines(device.PipelineCache(), { pipelineInfo }, nalloc);
		pipeline = pipelines[0];


		// Create descriptor pool
		vk::DescriptorPoolSize descriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1);

		vk::DescriptorPoolCreateInfo descriptorPoolInfo;
		descriptorPoolInfo.poolSizeCount(1);
		descriptorPoolInfo.pPoolSizes(&descriptorPoolSize);
		descriptorPoolInfo.maxSets(1);

		descriptorPool = device->createDescriptorPool(descriptorPoolInfo, nalloc);

		// Update descriptor sets with binding points
		vk::DescriptorSetAllocateInfo descSetInfo;
		descSetInfo.descriptorPool(descriptorPool);
		descSetInfo.descriptorSetCount(1);
		descSetInfo.pSetLayouts(&descriptorSetLayout);

		auto descriptorSets = device->allocateDescriptorSets(descSetInfo);
		descriptorSet = descriptorSets[0];

		// Uniform buffer at binding 0
		vk::WriteDescriptorSet writeDescSet;
		writeDescSet.dstSet(descriptorSet);
		writeDescSet.descriptorCount(1);
		writeDescSet.descriptorType(vk::DescriptorType::eUniformBuffer);
		auto &unifs = shaderSet->Get<TriangleVS>()->uniforms;
		writeDescSet.pBufferInfo(&unifs["ubo"].desc);
		writeDescSet.dstBinding(0);

		device->updateDescriptorSets({ writeDescSet }, {});


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
			prePresentBarrier.dstAccessMask({});
			prePresentBarrier.oldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			prePresentBarrier.newLayout(vk::ImageLayout::ePresentSrcKHR);
			prePresentBarrier.subresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			prePresentBarrier.image(swapchainImages[i]);

			cmdbuf.pipelineBarrier(
				vk::PipelineStageFlagBits::eAllCommands,
				vk::PipelineStageFlagBits::eTopOfPipe,
				{}, {}, {}, { prePresentBarrier }
			);

			cmdbuf.end();
		}

		semaphores.presentComplete = device->createSemaphore({}, nalloc);
		semaphores.renderComplete = device->createSemaphore({}, nalloc);
	}

	void Renderer::RenderFrame()
	{
		auto triangleVS = shaderSet->Get<TriangleVS>();

		static float rot = 0;
		triangleVS->transform.view = glm::rotate(glm::mat4(), rot, glm::vec3(0.f, 0.f, 1.f)) * glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, -2.5f));
		rot += 0.001;

		for (auto &entry : shaderSet->Map())
		{
			auto shader = entry.second;
			shader->UploadUniforms();
		}

		device->waitIdle();
		device->acquireNextImageKHR(vkswapchain, UINT64_MAX, semaphores.presentComplete, {}, currentBuffer);

		vk::SubmitInfo submitInfo;
		submitInfo.waitSemaphoreCount(1);
		submitInfo.pWaitSemaphores(&semaphores.presentComplete);
		submitInfo.signalSemaphoreCount(1);
		submitInfo.pSignalSemaphores(&semaphores.renderComplete);
		submitInfo.commandBufferCount(1);
		submitInfo.pCommandBuffers(&drawCmdBuffers[currentBuffer]);
		device.PrimaryQueue()->Handle().submit({ submitInfo }, {});

		vk::PresentInfoKHR presentInfo;
		presentInfo.waitSemaphoreCount(1);
		presentInfo.pWaitSemaphores(&semaphores.renderComplete);
		presentInfo.swapchainCount(1);
		presentInfo.pSwapchains(&vkswapchain);
		presentInfo.pImageIndices(&currentBuffer);
		device.PrimaryQueue()->Handle().presentKHR(presentInfo);

		vk::ImageMemoryBarrier postPresentBarrier;
		postPresentBarrier.srcAccessMask({});
		postPresentBarrier.dstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
		postPresentBarrier.oldLayout(vk::ImageLayout::ePresentSrcKHR);
		postPresentBarrier.newLayout(vk::ImageLayout::eColorAttachmentOptimal);
		postPresentBarrier.subresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
		postPresentBarrier.image(swapchainImages[currentBuffer]);

		postPresentCmdBuffer.begin({});
		postPresentCmdBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eAllCommands,
			vk::PipelineStageFlagBits::eTopOfPipe,
			{}, {}, {}, { postPresentBarrier }
		);
		postPresentCmdBuffer.end();

		vk::SubmitInfo postSubmitInfo;
		postSubmitInfo.commandBufferCount(1);
		postSubmitInfo.pCommandBuffers(&postPresentCmdBuffer);
		device.PrimaryQueue()->Handle().submit({ postSubmitInfo }, {});

		device->waitIdle();
	}
}
