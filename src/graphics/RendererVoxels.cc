#include "graphics/Renderer.hh"
#include "graphics/RenderTargetPool.hh"
#include "graphics/SceneShaders.hh"
#include "graphics/GPUTimer.hh"
#include "core/Game.hh"

namespace sp
{
	static CVar<float> CVarLightAttenuation("r.LightAttenuation", 0.5, "Light attenuation for voxel bounces");
	static CVar<float> CVarMaxVoxelFill("r.MaxVoxelFill", 0.5, "Maximum percentage of voxels that can be filled");

	std::atomic_bool printGfxDebug = false;
	static CFunc<void> CFuncList("printgfx", "Print the graphics debug output", []()
	{
		printGfxDebug = true;
	});

	void Renderer::PrepareVoxelTextures()
	{
		auto VoxelGridSize = voxelData.info.gridSize;
		auto VoxelListSize = VoxelGridSize * VoxelGridSize * VoxelGridSize * CVarMaxVoxelFill.Get();

		glm::ivec3 gridDimensions = glm::ivec3(VoxelGridSize, VoxelGridSize, VoxelGridSize);
		auto VoxelMipLevels = ceil(log2(VoxelGridSize));

		// { listSize, indirect_x, indirect_y, indirect_z }
		GLsizei atomicBufferSize = sizeof(GLuint) * 4 * VoxelMipLevels;
		if (!atomicCounterBuffer) atomicCounterBuffer.Create();
		if (atomicCounterBuffer.size != atomicBufferSize)
		{
			atomicCounterBuffer.Data(atomicBufferSize, nullptr, GL_DYNAMIC_COPY);

			GLuint listData[] = {0, 0, 1, 1};
			atomicCounterBuffer.Clear(PF_RGBA32UI, listData);
		}

		RenderTargetDesc listDesc(PF_RGB10_A2UI, glm::ivec2(8192, ceil(VoxelListSize / 8192.0)));
		listDesc.levels = VoxelMipLevels;
		if (!voxelData.fragmentList || voxelData.fragmentList->GetDesc() != listDesc)
		{
			voxelData.fragmentList = RTPool->Get(listDesc);
		}

		RenderTargetDesc counterDesc(PF_R32UI, gridDimensions);
		if (!voxelData.voxelCounters || voxelData.voxelCounters->GetDesc() != counterDesc)
		{
			voxelData.voxelCounters = RTPool->Get(counterDesc);
			voxelData.voxelCounters->GetTexture().Clear(0);
		}

		RenderTargetDesc overflowDesc(PF_RGBA16F, glm::ivec2(8192, ceil(VoxelListSize / 8192.0)));
		overflowDesc.levels = VoxelMipLevels;
		if (!voxelData.fragmentList || voxelData.fragmentList->GetDesc() != overflowDesc)
		{
			voxelData.voxelOverflow = RTPool->Get(overflowDesc);
		}

		RenderTargetDesc radianceDesc(PF_RGBA16F, gridDimensions);
		radianceDesc.wrapS = radianceDesc.wrapT = radianceDesc.wrapR = GL_CLAMP_TO_BORDER;
		radianceDesc.borderColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		if (!voxelData.radiance || voxelData.radiance->GetDesc() != radianceDesc)
		{
			voxelData.radiance = RTPool->Get(radianceDesc);
			voxelData.radiance->GetTexture().Clear(0);
		}

		auto mipSize = gridDimensions / 2;
		mipSize.x *= MAX_VOXEL_AREAS;

		RenderTargetDesc radianceMipsDesc(PF_RGBA16F, mipSize);
		radianceMipsDesc.levels = VoxelMipLevels - 1;
		radianceMipsDesc.wrapS = radianceDesc.wrapT = radianceDesc.wrapR = GL_CLAMP_TO_BORDER;
		radianceMipsDesc.borderColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		if (!voxelData.radianceMips || voxelData.radianceMips->GetDesc() != radianceMipsDesc)
		{
			voxelData.radianceMips = RTPool->Get(radianceMipsDesc);

			for (uint32 i = 0; i < VoxelMipLevels - 1; i++)
			{
				voxelData.radianceMips->GetTexture().Clear(0, i);
			}
		}
	}

	void Renderer::RenderVoxelGrid()
	{
		RenderPhase phase("VoxelGrid", Timer);

		PrepareVoxelTextures();

		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

		auto VoxelGridSize = voxelData.info.gridSize;

		ecs::View ortho;
		ortho.viewMat = glm::scale(glm::mat4(), glm::vec3(2.0 / (VoxelGridSize * voxelData.info.voxelSize)));
		ortho.viewMat = glm::translate(ortho.viewMat, -voxelData.info.voxelGridCenter);
		ortho.projMat = glm::mat4();
		ortho.extents = glm::ivec2(VoxelGridSize * voxelData.info.superSampleScale);
		ortho.clearMode = 0;

		auto renderTarget = RTPool->Get(RenderTargetDesc(PF_R8, ortho.extents));
		SetRenderTarget(renderTarget, nullptr);

		auto voxelVS = GlobalShaders->Get<VoxelFillVS>();

		{
			RenderPhase phase("Clear", Timer);

			atomicCounterBuffer.Bind(GL_DISPATCH_INDIRECT_BUFFER);

			for (uint32 i = 0; i <= voxelData.radianceMips->GetDesc().levels; i++)
			{
				atomicCounterBuffer.Bind(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 4 * i, sizeof(GLuint));
				voxelData.fragmentList->GetTexture().BindImage(0, GL_READ_ONLY, i);
				if (i == 0)
				{
					voxelData.radiance->GetTexture().BindImage(1, GL_WRITE_ONLY, 0, GL_TRUE, 0);
				}
				else
				{
					voxelData.radianceMips->GetTexture().BindImage(1, GL_WRITE_ONLY, i - 1, GL_TRUE, 0);
				}

				ShaderControl->BindPipeline<VoxelClearCS>(GlobalShaders);
				GlobalShaders->Get<VoxelClearCS>()->SetLevel(i);
				glDispatchComputeIndirect(sizeof(GLuint) * 4 * i + sizeof(GLuint));
			}

			GLuint listData[] = {0, 0, 1, 1};
			atomicCounterBuffer.Clear(PF_RGBA32UI, listData);
			voxelData.voxelCounters->GetTexture().Clear(0);

			glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		}

		{
			RenderPhase phase("Fill", Timer);

			atomicCounterBuffer.Bind(GL_ATOMIC_COUNTER_BUFFER, 0);
			voxelData.voxelCounters->GetTexture().BindImage(0, GL_READ_WRITE, 0, GL_TRUE, 0);
			voxelData.fragmentList->GetTexture().BindImage(1, GL_WRITE_ONLY, 0);
			voxelData.radiance->GetTexture().BindImage(2, GL_WRITE_ONLY, 0, GL_TRUE, 0);
			voxelData.voxelOverflow->GetTexture().BindImage(3, GL_WRITE_ONLY, 0);
			voxelData.voxelOverflow->GetTexture().BindImage(4, GL_WRITE_ONLY, 1);
			voxelData.voxelOverflow->GetTexture().BindImage(5, GL_WRITE_ONLY, 2);

			GLLightData lightData[MAX_LIGHTS];
			GLVoxelInfo voxelInfo;
			int lightCount = FillLightData(&lightData[0], game->entityManager);
			FillVoxelInfo(&voxelInfo, voxelData.info);

			auto voxelFillFS = GlobalShaders->Get<VoxelFillFS>();
			voxelFillFS->SetLightData(lightCount, &lightData[0]);
			voxelFillFS->SetVoxelInfo(&voxelInfo);
			voxelFillFS->SetLightAttenuation(CVarLightAttenuation.Get());

			shadowMap->GetTexture().Bind(4);
			if (mirrorShadowMap) mirrorShadowMap->GetTexture().Bind(5);
			if (menuGuiTarget) menuGuiTarget->GetTexture().Bind(6); // TODO(xthexder): bind correct light gel
			voxelData.radiance->GetTexture().Bind(7);
			voxelData.radianceMips->GetTexture().Bind(8);
			mirrorVisData.Bind(GL_SHADER_STORAGE_BUFFER, 0);

			ShaderControl->BindPipeline<VoxelFillVS, VoxelFillGS, VoxelFillFS>(GlobalShaders);
			ForwardPass(ortho, voxelVS);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
		}

		if (printGfxDebug)
		{
			uint32 *buf = (uint32 *) atomicCounterBuffer.Map(GL_READ_ONLY);
			Logf("Size: %d %d %d %d", buf[0], buf[4], buf[8], buf[12]);
			Logf("Compute count: %d %d %d %d", buf[1], buf[5], buf[9], buf[13]);
			atomicCounterBuffer.Unmap();

			atomicCounterBuffer.Delete();
			PrepareVoxelTextures();

			printGfxDebug = false;
		}

		{
			RenderPhase phase("Merge", Timer);

			atomicCounterBuffer.Bind(GL_DISPATCH_INDIRECT_BUFFER);

			// TODO(xthexder): Make last bucket sequencial to eliminate flickering
			for (uint32 i = 0; i < 3; i++)
			{
				atomicCounterBuffer.Bind(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 4 * (i + 1), sizeof(GLuint));
				voxelData.radiance->GetTexture().BindImage(0, GL_READ_WRITE, 0, GL_TRUE, 0);
				voxelData.voxelOverflow->GetTexture().BindImage(1, GL_READ_ONLY, i);

				ShaderControl->BindPipeline<VoxelMergeCS>(GlobalShaders);
				GlobalShaders->Get<VoxelMergeCS>()->SetLevel(i);
				glDispatchComputeIndirect(sizeof(GLuint) * 4 * (i + 1) + sizeof(GLuint));
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
			}

			GLuint listData[] = {0, 0, 1, 1};
			atomicCounterBuffer.ClearRegion(PF_RGBA32UI, sizeof(GLuint) * 4, -1, listData);
		}

		{
			RenderPhase phase("Mipmap", Timer);

			atomicCounterBuffer.Bind(GL_DISPATCH_INDIRECT_BUFFER);

			GLVoxelInfo voxelInfo;
			FillVoxelInfo(&voxelInfo, voxelData.info);

			auto voxelMipmapCS = GlobalShaders->Get<VoxelMipmapCS>();
			voxelMipmapCS->SetVoxelInfo(&voxelInfo);

			ShaderControl->BindPipeline<VoxelMipmapCS>(GlobalShaders);

			for (uint32 i = 0; i < voxelData.radianceMips->GetDesc().levels; i++)
			{
				RenderPhase phase("MipmapLevel", Timer);
				atomicCounterBuffer.Bind(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 4 * i, sizeof(GLuint) * 8);
				voxelData.fragmentList->GetTexture().BindImage(0, GL_READ_ONLY, i);
				voxelData.fragmentList->GetTexture().BindImage(1, GL_READ_WRITE, i + 1);
				if (i == 0)
					voxelData.radiance->GetTexture().BindImage(2, GL_READ_ONLY, 0, GL_TRUE, 0);
				else
					voxelData.radianceMips->GetTexture().BindImage(2, GL_READ_ONLY, i - 1, GL_TRUE, 0);
				voxelData.radianceMips->GetTexture().BindImage(3, GL_WRITE_ONLY, i, GL_TRUE, 0);

				voxelMipmapCS->SetLevel(i);

				glDispatchComputeIndirect(sizeof(GLuint) * 4 * i + sizeof(GLuint));
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
			}
		}

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
	}
}
