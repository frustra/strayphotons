#include "graphics/Renderer.hh"
#include "graphics/RenderTargetPool.hh"
#include "graphics/SceneShaders.hh"
#include "graphics/GPUTimer.hh"
#include "core/Game.hh"

namespace sp
{
	static CVar<float> CVarLightAttenuation("r.LightAttenuation", 0.5, "Light attenuation for voxel bounces");

	void Renderer::PrepareVoxelTextures()
	{
		auto VoxelGridSize = voxelData.info.gridSize;

		glm::ivec3 packedSize = glm::ivec3(VoxelGridSize * 3, VoxelGridSize, VoxelGridSize);
		glm::ivec3 unpackedSize = glm::ivec3(VoxelGridSize, VoxelGridSize, VoxelGridSize);
		auto VoxelMipLevels = ceil(log2(VoxelGridSize));

		auto indirectBufferSize = sizeof(GLuint) * 4 * VoxelMipLevels;

		if (!computeIndirectBuffer1) computeIndirectBuffer1.Create();
		if (!computeIndirectBuffer2) computeIndirectBuffer2.Create();
		if (computeIndirectBuffer1.size != indirectBufferSize || computeIndirectBuffer2.size != indirectBufferSize)
		{
			computeIndirectBuffer1.Data(indirectBufferSize, nullptr, GL_DYNAMIC_COPY);
			computeIndirectBuffer2.Data(indirectBufferSize, nullptr, GL_DYNAMIC_COPY);

			GLuint listData[] = {0, 0, 1, 1};
			computeIndirectBuffer1.Clear(PF_RGBA32UI, listData);
			computeIndirectBuffer2.Clear(PF_RGBA32UI, listData);
		}

		auto VoxelListSize = VoxelGridSize * VoxelGridSize * VoxelGridSize / 4;
		RenderTargetDesc listDesc(PF_R32UI, glm::ivec2(8192, ceil(VoxelListSize / 8192.0)));
		listDesc.levels = VoxelMipLevels;
		if (!voxelData.fragmentList1 || voxelData.fragmentList1->GetDesc() != listDesc)
		{
			voxelData.fragmentList1 = RTPool->Get(listDesc);
		}
		if (!voxelData.fragmentList2 || voxelData.fragmentList2->GetDesc() != listDesc)
		{
			voxelData.fragmentList2 = RTPool->Get(listDesc);
		}

		if (!voxelData.packedData || voxelData.packedData->GetDesc().extent != packedSize)
		{
			voxelData.packedData = RTPool->Get(RenderTargetDesc(PF_R32UI, packedSize));
			voxelData.packedData->GetTexture().Clear(0);
		}

		RenderTargetDesc radianceDesc(PF_RGBA16, unpackedSize);
		radianceDesc.wrapS = radianceDesc.wrapT = radianceDesc.wrapR = GL_CLAMP_TO_BORDER;
		radianceDesc.borderColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		if (!voxelData.radiance || voxelData.radiance->GetDesc() != radianceDesc)
		{
			voxelData.radiance = RTPool->Get(radianceDesc);
			voxelData.radiance->GetTexture().Clear(0);
		}

		auto mipSize = unpackedSize / 2;
		mipSize.x *= MAX_VOXEL_AREAS + 1;

		RenderTargetDesc radianceMipsDesc(PF_RGBA16, mipSize);
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
		SetRenderTarget(&renderTarget->GetTexture(), nullptr);

		auto voxelVS = GlobalShaders->Get<VoxelRasterVS>();

		auto computeIndirectBuffer = voxelFlipflop ? computeIndirectBuffer1 : computeIndirectBuffer2;
		auto lastComputeIndirectBuffer = voxelFlipflop ? computeIndirectBuffer2 : computeIndirectBuffer1;
		auto fragmentList = voxelFlipflop ? voxelData.fragmentList1 : voxelData.fragmentList2;
		auto lastFragmentList = voxelFlipflop ? voxelData.fragmentList2 : voxelData.fragmentList1;
		voxelFlipflop = !voxelFlipflop;

		{
			RenderPhase phase("Accumulate", Timer);
			computeIndirectBuffer.Bind(GL_ATOMIC_COUNTER_BUFFER, 0);
			fragmentList->GetTexture().BindImage(0, GL_WRITE_ONLY, 0);
			voxelData.packedData->GetTexture().BindImage(1, GL_READ_WRITE, 0, GL_TRUE, 0);

			GLLightData lightData[MAX_LIGHTS];
			GLVoxelInfo voxelInfo;
			int lightCount = FillLightData(&lightData[0], game->entityManager);
			FillVoxelInfo(&voxelInfo, voxelData.info);

			auto voxelRasterFS = GlobalShaders->Get<VoxelRasterFS>();
			voxelRasterFS->SetLightData(lightCount, &lightData[0]);
			voxelRasterFS->SetVoxelInfo(&voxelInfo);
			voxelRasterFS->SetLightAttenuation(CVarLightAttenuation.Get());

			shadowMap->GetTexture().Bind(4);
			if (mirrorShadowMap) mirrorShadowMap->GetTexture().Bind(5);
			if (menuGuiTarget) menuGuiTarget->GetTexture().Bind(6); // TODO(xthexder): bind correct light gel
			voxelData.radiance->GetTexture().Bind(7);
			voxelData.radianceMips->GetTexture().Bind(8);
			mirrorVisData.Bind(GL_SHADER_STORAGE_BUFFER, 0);

			ShaderControl->BindPipeline<VoxelRasterVS, VoxelRasterGS, VoxelRasterFS>(GlobalShaders);
			ForwardPass(ortho, voxelVS);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
		}

		{
			RenderPhase phase("VoxelClear", Timer);

			lastComputeIndirectBuffer.Bind(GL_DISPATCH_INDIRECT_BUFFER);

			for (uint32 i = 0; i < voxelData.radianceMips->GetDesc().levels + 1; i++)
			{
				lastComputeIndirectBuffer.Bind(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 4 * i, sizeof(GLuint));
				lastFragmentList->GetTexture().BindImage(0, GL_READ_ONLY, i);
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
			lastComputeIndirectBuffer.Clear(PF_RGBA32UI, listData);

			glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		}

		computeIndirectBuffer.Bind(GL_DISPATCH_INDIRECT_BUFFER);

		{
			RenderPhase phase("Convert", Timer);
			computeIndirectBuffer.Bind(GL_ATOMIC_COUNTER_BUFFER, 0);
			fragmentList->GetTexture().BindImage(0, GL_READ_ONLY, 0);
			voxelData.packedData->GetTexture().BindImage(1, GL_READ_WRITE, 0, GL_TRUE, 0);
			voxelData.radiance->GetTexture().BindImage(2, GL_WRITE_ONLY, 0, GL_TRUE, 0);

			ShaderControl->BindPipeline<VoxelConvertCS>(GlobalShaders);
			glDispatchComputeIndirect(sizeof(GLuint));
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
		}
		{
			GLVoxelInfo voxelInfo;
			FillVoxelInfo(&voxelInfo, voxelData.info);

			auto voxelMipmapCS = GlobalShaders->Get<VoxelMipmapCS>();
			voxelMipmapCS->SetVoxelInfo(&voxelInfo);

			ShaderControl->BindPipeline<VoxelMipmapCS>(GlobalShaders);

			for (uint32 i = 1; i < voxelData.radianceMips->GetDesc().levels + 1; i++)
			{
				RenderPhase phase("Mipmap", Timer);
				computeIndirectBuffer.Bind(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 4 * (i - 1), sizeof(GLuint) * 8);
				fragmentList->GetTexture().BindImage(0, GL_READ_ONLY, i - 1);
				fragmentList->GetTexture().BindImage(1, GL_READ_WRITE, i);
				if (i == 1)
					voxelData.radiance->GetTexture().BindImage(2, GL_READ_ONLY, 0, GL_TRUE, 0);
				else
					voxelData.radianceMips->GetTexture().BindImage(2, GL_READ_ONLY, i - 2, GL_TRUE, 0);
				voxelData.radianceMips->GetTexture().BindImage(3, GL_WRITE_ONLY, i - 1, GL_TRUE, 0);

				voxelMipmapCS->SetLevel(i);

				glDispatchComputeIndirect(sizeof(GLuint) * 4 * (i - 1) + sizeof(GLuint));
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
			}
		}

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
	}
}
