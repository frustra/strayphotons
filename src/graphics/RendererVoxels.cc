#include "core/Game.hh"
#include "core/PerfTimer.hh"
#include "graphics/RenderTargetPool.hh"
#include "graphics/Renderer.hh"
#include "graphics/SceneShaders.hh"

#include <atomic>

namespace sp {
	static CVar<float> CVarLightAttenuation("r.LightAttenuation", 0.5, "Light attenuation for voxel bounces");
	static CVar<float> CVarMaxVoxelFill("r.MaxVoxelFill", 0.5, "Maximum percentage of voxels that can be filled");

	std::atomic_bool printGfxDebug = false;
	static CFunc<void> CFuncList("printgfx", "Print the graphics debug output", []() {
		printGfxDebug = true;
	});

	void Renderer::PrepareVoxelTextures() {
		auto VoxelGridSize = voxelData.info.gridSize;
		auto VoxelListSize = VoxelGridSize * VoxelGridSize * VoxelGridSize * CVarMaxVoxelFill.Get();

		glm::ivec3 gridDimensions = glm::ivec3(VoxelGridSize, VoxelGridSize, VoxelGridSize);
		auto VoxelMipLevels = ceil(log2(VoxelGridSize));

		// { listSize, indirect_x, indirect_y, indirect_z }
		GLsizei indirectBufferSize = sizeof(GLuint) * 4 * VoxelMipLevels;
		if (!indirectBufferCurrent) indirectBufferCurrent.Create();
		if (!indirectBufferPrevious) indirectBufferPrevious.Create();
		if (indirectBufferCurrent.size != indirectBufferSize || indirectBufferPrevious.size != indirectBufferSize) {
			indirectBufferCurrent.Data(indirectBufferSize, nullptr, GL_DYNAMIC_COPY);
			indirectBufferPrevious.Data(indirectBufferSize, nullptr, GL_DYNAMIC_COPY);

			GLuint listData[] = {0, 0, 1, 1};
			indirectBufferCurrent.Clear(PF_RGBA32UI, listData);
			indirectBufferPrevious.Clear(PF_RGBA32UI, listData);
		}

		RenderTargetDesc listDesc(PF_RGB10_A2UI, glm::ivec2(8192, ceil(VoxelListSize / 8192.0)));
		listDesc.levels = VoxelMipLevels;
		listDesc.Prepare(RTPool, voxelData.fragmentListCurrent);
		listDesc.Prepare(RTPool, voxelData.fragmentListPrevious);

		RenderTargetDesc counterDesc(PF_R32UI, gridDimensions);
		counterDesc.levels = VoxelMipLevels;
		counterDesc.Prepare(RTPool, voxelData.voxelCounters, true);

		RenderTargetDesc overflowDesc(PF_RGBA16F, glm::ivec2(8192, ceil(VoxelListSize / 8192.0)));
		overflowDesc.levels = VoxelMipLevels;
		overflowDesc.Prepare(RTPool, voxelData.voxelOverflow);

		RenderTargetDesc radianceDesc(PF_RGBA16F, gridDimensions);
		radianceDesc.Wrap(GL_CLAMP_TO_BORDER);
		radianceDesc.borderColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		radianceDesc.Prepare(RTPool, voxelData.radiance, true);

		auto mipSize = gridDimensions / 2;
		mipSize.x *= MAX_VOXEL_AREAS;

		RenderTargetDesc radianceMipsDesc(PF_RGBA16F, mipSize);
		radianceMipsDesc.levels = VoxelMipLevels - 1;
		radianceMipsDesc.Wrap(GL_CLAMP_TO_BORDER);
		radianceMipsDesc.borderColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		radianceMipsDesc.Prepare(RTPool, voxelData.radianceMips, true);
	}

	void Renderer::RenderVoxelGrid() {
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

		GLVoxelInfo voxelInfo;
		FillVoxelInfo(&voxelInfo, voxelData.info);
		GLLightData lightData[MAX_LIGHTS];
		int lightCount = FillLightData(&lightData[0], game->entityManager);

		{
			RenderPhase phase("Fill", Timer);

			indirectBufferCurrent.Bind(GL_ATOMIC_COUNTER_BUFFER, 0);
			voxelData.voxelCounters->GetTexture().BindImage(0, GL_READ_WRITE, 0, GL_TRUE, 0);
			voxelData.fragmentListCurrent->GetTexture().BindImage(1, GL_WRITE_ONLY, 0);
			voxelData.radiance->GetTexture().BindImage(2, GL_WRITE_ONLY, 0, GL_TRUE, 0);
			voxelData.voxelOverflow->GetTexture().BindImage(3, GL_WRITE_ONLY, 0);
			voxelData.voxelOverflow->GetTexture().BindImage(4, GL_WRITE_ONLY, 1);
			voxelData.voxelOverflow->GetTexture().BindImage(5, GL_WRITE_ONLY, 2);

			shadowMap->GetTexture().Bind(4);
			if (mirrorShadowMap) mirrorShadowMap->GetTexture().Bind(5);
			if (menuGuiTarget) menuGuiTarget->GetTexture().Bind(6); // TODO(xthexder): bind correct light gel
			voxelData.radiance->GetTexture().Bind(7);
			voxelData.radianceMips->GetTexture().Bind(8);
			mirrorVisData.Bind(GL_SHADER_STORAGE_BUFFER, 0);

			ShaderControl->BindPipeline<VoxelFillVS, VoxelFillGS, VoxelFillFS>(GlobalShaders);
			auto voxelFillFS = GlobalShaders->Get<VoxelFillFS>();
			voxelFillFS->SetLightData(lightCount, &lightData[0]);
			voxelFillFS->SetVoxelInfo(&voxelInfo);
			voxelFillFS->SetLightAttenuation(CVarLightAttenuation.Get());

			ForwardPass(ortho, GlobalShaders->Get<VoxelFillVS>());
			glMemoryBarrier(
				GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
		}

		{
			RenderPhase phase("Merge", Timer);

			indirectBufferCurrent.Bind(GL_DISPATCH_INDIRECT_BUFFER);

			// TODO(xthexder): Make last bucket sequencial to eliminate flickering
			for (uint32 i = 0; i < 3; i++) {
				indirectBufferCurrent.Bind(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 4 * (i + 1), sizeof(GLuint));
				voxelData.radiance->GetTexture().BindImage(0, GL_READ_WRITE, 0, GL_TRUE, 0);
				voxelData.voxelOverflow->GetTexture().BindImage(1, GL_READ_ONLY, i);

				ShaderControl->BindPipeline<VoxelMergeCS>(GlobalShaders);
				GlobalShaders->Get<VoxelMergeCS>()->SetLevel(i);
				glDispatchComputeIndirect(sizeof(GLuint) * 4 * (i + 1) + sizeof(GLuint));
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			}

			GLuint listData[] = {0, 0, 1, 1};
			indirectBufferCurrent.ClearRegion(PF_RGBA32UI, sizeof(GLuint) * 4, -1, listData);
			glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
		}

		{
			RenderPhase phase("Mipmap", Timer);

			for (uint32 i = 0; i <= voxelData.radianceMips->GetDesc().levels; i++) {
				{
					RenderPhase subPhase("Clear", Timer);

					indirectBufferPrevious.Bind(GL_DISPATCH_INDIRECT_BUFFER);
					indirectBufferPrevious.Bind(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 4 * i, sizeof(GLuint));
					voxelData.fragmentListPrevious->GetTexture().BindImage(0, GL_READ_ONLY, i);
					voxelData.voxelCounters->GetTexture().BindImage(1, GL_READ_ONLY, i);
					if (i == 0) {
						voxelData.radiance->GetTexture().BindImage(2, GL_WRITE_ONLY, 0, GL_TRUE, 0);
					} else {
						voxelData.radianceMips->GetTexture().BindImage(2, GL_WRITE_ONLY, i - 1, GL_TRUE, 0);
					}

					ShaderControl->BindPipeline<VoxelClearCS>(GlobalShaders);
					GlobalShaders->Get<VoxelClearCS>()->SetLevel(i);
					glDispatchComputeIndirect(sizeof(GLuint) * 4 * i + sizeof(GLuint));
					glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
				}

				{
					RenderPhase subPhase("MipmapLevel", Timer);

					indirectBufferCurrent.Bind(GL_DISPATCH_INDIRECT_BUFFER);
					indirectBufferCurrent.Bind(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 4 * i, sizeof(GLuint) * 4);
					voxelData.fragmentListCurrent->GetTexture().BindImage(0, GL_READ_ONLY, i);
					voxelData.voxelCounters->GetTexture().BindImage(2, GL_WRITE_ONLY, i);
					if (i < voxelData.radianceMips->GetDesc().levels) {
						indirectBufferCurrent.Bind(GL_ATOMIC_COUNTER_BUFFER,
							1,
							sizeof(GLuint) * 4 * (i + 1),
							sizeof(GLuint) * 4);
						voxelData.voxelCounters->GetTexture().BindImage(3, GL_READ_WRITE, i + 1);
						voxelData.fragmentListCurrent->GetTexture().BindImage(1, GL_WRITE_ONLY, i + 1);
					}
					if (i > 0) {
						if (i > 1) voxelData.radianceMips->GetTexture().BindImage(4, GL_READ_ONLY, i - 2, GL_TRUE, 0);
						else
							voxelData.radiance->GetTexture().BindImage(4, GL_READ_ONLY, 0, GL_TRUE, 0);
						voxelData.radianceMips->GetTexture().BindImage(5, GL_WRITE_ONLY, i - 1, GL_TRUE, 0);
					}

					ShaderControl->BindPipeline<VoxelMipmapCS>(GlobalShaders);
					auto voxelMipmapCS = GlobalShaders->Get<VoxelMipmapCS>();
					voxelMipmapCS->SetVoxelInfo(&voxelInfo);
					voxelMipmapCS->SetLevel(i);

					glDispatchComputeIndirect(sizeof(GLuint) * 4 * i + sizeof(GLuint));
					glMemoryBarrier(
						GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
				}
			}
			glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		}

		if (printGfxDebug) {
			uint32 *buf = (uint32 *)indirectBufferCurrent.Map(GL_READ_ONLY);
			Logf("Size: %d %d %d %d", buf[0], buf[4], buf[8], buf[12]);
			Logf("Compute count: %d %d %d %d", buf[1], buf[5], buf[9], buf[13]);
			indirectBufferCurrent.Unmap();

			indirectBufferCurrent.Delete();
			PrepareVoxelTextures();

			printGfxDebug = false;
		}

		{
			RenderPhase phase("Swap", Timer);

			Buffer tmp = indirectBufferPrevious;
			indirectBufferPrevious = indirectBufferCurrent;
			indirectBufferCurrent = tmp;

			shared_ptr<RenderTarget> tmp2 = voxelData.fragmentListPrevious;
			voxelData.fragmentListPrevious = voxelData.fragmentListCurrent;
			voxelData.fragmentListCurrent = tmp2;

			GLuint listData[] = {0, 0, 1, 1};
			indirectBufferCurrent.Clear(PF_RGBA32UI, listData);
		}

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
	}
} // namespace sp
