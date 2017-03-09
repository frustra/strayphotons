#include "graphics/Renderer.hh"
#include "graphics/RenderTargetPool.hh"
#include "graphics/Shader.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/Util.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/SceneShaders.hh"
#include "graphics/LightSensor.hh"
#include "graphics/GPUTimer.hh"
#include "graphics/GPUTypes.hh"
#include "graphics/postprocess/PostProcess.hh"
#include "graphics/GuiRenderer.hh"
#include "core/Game.hh"
#include "core/Logging.hh"
#include "core/CVar.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/View.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/VoxelInfo.hh"
#include "ecs/components/Mirror.hh"
#include "assets/AssetManager.hh"
#include "physx/PhysxUtils.hh"
#include "threading/MutexedVector.hh"

#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cxxopts.hpp>

namespace sp
{
	Renderer::Renderer(Game *game) : GraphicsContext(game)
	{
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	}

	Renderer::~Renderer()
	{
		if (ShaderControl)
			delete ShaderControl;
		if (RTPool)
			delete RTPool;
	}

	const int MAX_MIRROR_RECURSION = 10;

	static CVar<bool> CVarRenderWireframe("r.Wireframe", false, "Render wireframes");
	static CVar<bool> CVarUpdateVoxels("r.UpdateVoxels", true, "Render voxel grid each frame");
	static CVar<int> CVarMirrorRecursion("r.MirrorRecursion", 2, "Mirror recursion depth");
	static CVar<int> CVarMirrorMapResolution("r.MirrorMapResolution", 512, "Resolution of mirror shadow maps");
	static CVar<float> CVarLightAttenuation("r.LightAttenuation", 0.5, "Light attenuation for voxel bounces");

	static CVar<int> CVarVoxelGridSize("r.VoxelGridSize", 256, "NxNxN voxel grid dimensions");
	static CVar<float> CVarVoxelSuperSample("r.VoxelSuperSample", 1.0, "Render voxel grid with Nx supersampling");
	static CVar<bool> CVarEnableShadows("r.EnableShadows", true, "Enable shadow mapping");
	static CVar<bool> CVarEnablePCF("r.EnablePCF", true, "Enable smooth shadow sampling");

	void Renderer::UpdateShaders(bool force)
	{
		if (force ||
				CVarVoxelGridSize.Changed() ||
				CVarVoxelSuperSample.Changed() ||
				CVarEnableShadows.Changed() ||
				CVarEnablePCF.Changed()
		   )
		{
			ShaderManager::SetDefine("VOXEL_GRID_SIZE", std::to_string(CVarVoxelGridSize.Get(true)));
			ShaderManager::SetDefine("VOXEL_SUPER_SAMPLE_SCALE", std::to_string(CVarVoxelSuperSample.Get(true)));
			ShaderManager::SetDefine("SHADOWS_ENABLED", CVarEnableShadows.Get(true));
			ShaderManager::SetDefine("PCF_ENABLED", CVarEnablePCF.Get(true));
			ShaderControl->CompileAll(GlobalShaders);
		}
	}

	void Renderer::Prepare()
	{
		Assert(GLEW_ARB_compute_shader, "ARB_compute_shader required");
		Assert(GLEW_ARB_direct_state_access, "ARB_direct_state_access required");
		Assert(GLEW_ARB_multi_bind, "ARB_multi_bind required");

		glEnable(GL_FRAMEBUFFER_SRGB);

		RTPool = new RenderTargetPool();
		debugGuiRenderer = make_shared<GuiRenderer>(*this, &game->debugGui);
		menuGuiRenderer = make_shared<GuiRenderer>(*this, &game->menuGui);

		ShaderControl = new ShaderManager();
		ShaderManager::SetDefine("MAX_LIGHTS", std::to_string(MAX_LIGHTS));
		ShaderManager::SetDefine("MAX_MIRRORS", std::to_string(MAX_MIRRORS));
		ShaderManager::SetDefine("MAX_MIRROR_RECURSION", std::to_string(MAX_MIRROR_RECURSION));
		ShaderManager::SetDefine("MAX_LIGHT_SENSORS", std::to_string(MAX_LIGHT_SENSORS));
		UpdateShaders(true);

		game->entityManager.Subscribe<ecs::EntityDestruction>([&](ecs::Entity ent, const ecs::EntityDestruction & d)
		{
			if (ent.Has<ecs::Renderable>())
			{
				auto renderable = ent.Get<ecs::Renderable>();
				if (renderable->model->glModel)
				{
					// Keep GLModel objects around for at least 5 frames after destruction
					renderableGCQueue.push_back({renderable->model, 5});
				}
				renderable->model = nullptr;
			}
		});

		AssertGLOK("Renderer::Prepare");
	}

	void Renderer::RenderMainMenu(ecs::View &view, bool renderToGel)
	{
		if (renderToGel)
		{
			RenderTargetDesc menuDesc(PF_RGBA8, view.extents);
			menuDesc.levels = Texture::FullyMipmap;
			menuDesc.anisotropy = 4.0;
			if (!menuGuiTarget || menuGuiTarget->GetDesc() != menuDesc)
			{
				menuGuiTarget = RTPool->Get(menuDesc);
			}

			SetRenderTarget(&menuGuiTarget->GetTexture(), nullptr);
			PrepareForView(view);
			menuGuiRenderer->Render(view);
			menuGuiTarget->GetTexture().GenMipmap();
		}
		else
		{
			menuGuiRenderer->Render(view);
		}
	}

	void Renderer::RenderShadowMaps()
	{
		RenderPhase phase("ShadowMaps", Timer);

		glm::ivec2 renderTargetSize;
		int lightCount = 0;
		for (auto entity : game->entityManager.EntitiesWith<ecs::Light>())
		{
			auto light = entity.Get<ecs::Light>();
			if (entity.Has<ecs::View>())
			{
				auto view = ecs::UpdateViewCache(entity, light->spotAngle * 2.0f);
				light->mapOffset = glm::vec4(renderTargetSize.x, 0, view->extents.x, view->extents.y);
				light->lightId = lightCount++;
				view->offset = glm::ivec2(light->mapOffset);
				view->clearMode = 0;

				renderTargetSize.x += view->extents.x;
				if (view->extents.y > renderTargetSize.y)
					renderTargetSize.y = view->extents.y;
			}
		}

		int mirrorCount = 0;
		for (auto entity : game->entityManager.EntitiesWith<ecs::Mirror>())
		{
			auto mirror = entity.Get<ecs::Mirror>();
			mirror->mirrorId = mirrorCount++;
		}

		if (lightCount == 0) return;

		RenderTargetDesc shadowDesc(PF_R32F, renderTargetSize);
		if (!shadowMap || shadowMap->GetDesc() != shadowDesc)
		{
			shadowMap = RTPool->Get(shadowDesc);
		}

		if (!mirrorVisData)
		{
			// int count[4];
			// uint mask[MAX_LIGHTS * MAX_MIRRORS][MAX_MIRRORS];
			// uint list[MAX_LIGHTS * MAX_MIRRORS];
			// int sourceLight[MAX_LIGHTS * MAX_MIRRORS];
			// mat4 viewMat[MAX_LIGHTS * MAX_MIRRORS];
			// mat4 invViewMat[MAX_LIGHTS * MAX_MIRRORS];
			// mat4 projMat[MAX_LIGHTS * MAX_MIRRORS];
			// mat4 invProjMat[MAX_LIGHTS * MAX_MIRRORS];
			// mat4 lightViewMat[MAX_LIGHTS * MAX_MIRRORS];
			// mat4 invLightViewMat[MAX_LIGHTS * MAX_MIRRORS];
			// vec2 clip[MAX_LIGHTS * MAX_MIRRORS];
			// vec4 nearInfo[MAX_LIGHTS * MAX_MIRRORS];

			mirrorVisData.Create()
			.Data(sizeof(GLint) * 4 + (sizeof(GLuint) * MAX_MIRRORS + sizeof(GLuint) * 8 + sizeof(glm::mat4) * 6) * (MAX_LIGHTS * MAX_MIRRORS + 1 /* padding */), nullptr, GL_DYNAMIC_COPY);
		}

		if (CVarEnableShadows.Get())
		{
			auto depthTarget = RTPool->Get(RenderTargetDesc(PF_DEPTH16, renderTargetSize));
			SetRenderTarget(&shadowMap->GetTexture(), &depthTarget->GetTexture());

			glViewport(0, 0, renderTargetSize.x, renderTargetSize.y);
			glDisable(GL_SCISSOR_TEST);
			glEnable(GL_CULL_FACE);
			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE);
			glClear(GL_DEPTH_BUFFER_BIT);

			mirrorVisData.Clear(PF_R32UI, 0);
			mirrorVisData.Bind(GL_SHADER_STORAGE_BUFFER, 0);

			for (auto entity : game->entityManager.EntitiesWith<ecs::Light>())
			{
				auto light = entity.Get<ecs::Light>();
				if (entity.Has<ecs::View>())
				{
					light->mapOffset /= glm::vec4(renderTargetSize, renderTargetSize);

					ecs::Handle<ecs::View> view = entity.Get<ecs::View>();

					ShaderControl->BindPipeline<ShadowMapVS, ShadowMapFS>(GlobalShaders);

					auto shadowMapVS = GlobalShaders->Get<ShadowMapVS>();
					auto shadowMapFS = GlobalShaders->Get<ShadowMapFS>();
					shadowMapFS->SetClip(view->clip);
					shadowMapFS->SetLight(light->lightId);
					ForwardPass(*view, shadowMapVS, [&] (ecs::Entity & ent)
					{
						if (ent.Has<ecs::Mirror>())
						{
							auto mirror = ent.Get<ecs::Mirror>();
							shadowMapFS->SetMirrorId(mirror->mirrorId);
						}
						else
						{
							shadowMapFS->SetMirrorId(-1);
						}
					});
					glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
				}
			}
		}

		glDisable(GL_CULL_FACE);

		GLLightData lightData[MAX_LIGHTS];
		GLMirrorData mirrorData[MAX_MIRRORS];
		int lightDataCount = FillLightData(&lightData[0], game->entityManager);
		int mirrorDataCount = FillMirrorData(&mirrorData[0], game->entityManager);
		int recursion = mirrorCount == 0 ? 0 : CVarMirrorRecursion.Get();

		int mapCount = lightDataCount * mirrorDataCount * recursion;
		auto mapResolution = CVarMirrorMapResolution.Get();
		auto mirrorMapResolution = glm::ivec3(mapResolution, mapResolution, std::max(1, mapCount));
		RenderTargetDesc mirrorMapDesc(PF_R32F, mirrorMapResolution);
		mirrorMapDesc.textureArray = true;
		if (!mirrorShadowMap || mirrorShadowMap->GetDesc() != mirrorMapDesc)
		{
			mirrorShadowMap = RTPool->Get(mirrorMapDesc);
		}

		for (int bounce = 0; bounce < recursion; bounce++)
		{
			{
				RenderPhase phase("MatrixGen", Timer);

				auto mirrorMapCS = GlobalShaders->Get<MirrorMapCS>();

				mirrorMapCS->SetLightData(lightDataCount, &lightData[0]);
				mirrorMapCS->SetMirrorData(mirrorDataCount, &mirrorData[0]);

				ShaderControl->BindPipeline<MirrorMapCS>(GlobalShaders);
				glDispatchCompute(1, 1, 1);
				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			}

			{
				RenderPhase phase("MirrorMaps", Timer);

				RenderTargetDesc depthDesc(PF_DEPTH32F, mirrorMapResolution);
				depthDesc.textureArray = true;
				auto depthTarget = RTPool->Get(depthDesc);
				SetRenderTarget(&mirrorShadowMap->GetTexture(), &depthTarget->GetTexture());

				ecs::View basicView;
				basicView.offset = glm::ivec2(0);
				basicView.extents = glm::ivec2(mapResolution);

				if (bounce > 0)
					basicView.clearMode = 0;

				ShaderControl->BindPipeline<MirrorMapVS, MirrorMapGS, MirrorMapFS>(GlobalShaders);

				auto mirrorMapFS = GlobalShaders->Get<MirrorMapFS>();
				auto mirrorMapVS = GlobalShaders->Get<MirrorMapVS>();

				mirrorMapFS->SetLightData(lightDataCount, &lightData[0]);
				mirrorMapFS->SetMirrorId(-1);

				shadowMap->GetTexture().Bind(4);
				mirrorShadowMap->GetTexture().Bind(5);

				ForwardPass(basicView, mirrorMapVS, [&](ecs::Entity & ent)
				{
					if (bounce == recursion - 1)
					{
						// Don't mark mirrors on last pass.
					}
					else if (ent.Has<ecs::Mirror>())
					{
						auto mirror = ent.Get<ecs::Mirror>();
						mirrorMapFS->SetMirrorId(mirror->mirrorId);
					}
					else
					{
						mirrorMapFS->SetMirrorId(-1);
					}
				});
				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			}
		}
	}

	void Renderer::PrepareVoxelTextures()
	{
		auto VoxelGridSize = voxelData.info.gridSize;

		glm::ivec3 packedSize = glm::ivec3(VoxelGridSize * 3, VoxelGridSize, VoxelGridSize);
		glm::ivec3 unpackedSize = glm::ivec3(VoxelGridSize, VoxelGridSize, VoxelGridSize);
		auto VoxelMipLevels = ceil(log2(VoxelGridSize));

		auto indirectBufferSize = sizeof(GLuint) * 4 * VoxelMipLevels;

		if (!computeIndirectBuffer1) computeIndirectBuffer1.Create();
		if (!computeIndirectBuffer2) computeIndirectBuffer2.Create();
		if (computeIndirectBuffer1.size != indirectBufferSize)
		{
			computeIndirectBuffer1.Data(indirectBufferSize, nullptr, GL_DYNAMIC_COPY);

			GLuint listData[] = {0, 0, 1, 1};
			computeIndirectBuffer1.Clear(PF_RGBA32UI, listData);
		}
		if (computeIndirectBuffer2.size != indirectBufferSize)
		{
			computeIndirectBuffer2.Data(indirectBufferSize, nullptr, GL_DYNAMIC_COPY);

			GLuint listData[] = {0, 0, 1, 1};
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
		radianceDesc.levels = VoxelMipLevels;
		if (!voxelData.radiance || voxelData.radiance->GetDesc() != radianceDesc)
		{
			voxelData.radiance = RTPool->Get(radianceDesc);

			for (uint32 i = 0; i < VoxelMipLevels; i++)
			{
				voxelData.radiance->GetTexture().Clear(0, i);
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
			int lightCount = FillLightData(&lightData[0], game->entityManager);

			auto voxelRasterFS = GlobalShaders->Get<VoxelRasterFS>();
			voxelRasterFS->SetLightData(lightCount, &lightData[0]);
			voxelRasterFS->SetVoxelInfo(voxelData.info);
			voxelRasterFS->SetLightAttenuation(CVarLightAttenuation.Get());

			shadowMap->GetTexture().Bind(4);
			if (mirrorShadowMap) mirrorShadowMap->GetTexture().Bind(5);
			if (menuGuiTarget) menuGuiTarget->GetTexture().Bind(6); // TODO(xthexder): bind correct light gel
			voxelData.radiance->GetTexture().Bind(7);
			mirrorVisData.Bind(GL_SHADER_STORAGE_BUFFER, 0);

			ShaderControl->BindPipeline<VoxelRasterVS, VoxelRasterGS, VoxelRasterFS>(GlobalShaders);
			ForwardPass(ortho, voxelVS);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
		}

		{
			RenderPhase phase("VoxelClear", Timer);

			lastComputeIndirectBuffer.Bind(GL_DISPATCH_INDIRECT_BUFFER);

			for (uint32 i = 0; i < voxelData.radiance->GetDesc().levels; i++)
			{
				lastComputeIndirectBuffer.Bind(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 4 * i, sizeof(GLuint));
				lastFragmentList->GetTexture().BindImage(0, GL_READ_ONLY, i);
				voxelData.radiance->GetTexture().BindImage(1, GL_WRITE_ONLY, i, GL_TRUE, 0);

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
			RenderPhase phase("Mipmap", Timer);
			for (uint32 i = 1; i < voxelData.radiance->GetDesc().levels; i++)
			{
				computeIndirectBuffer.Bind(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 4 * (i - 1), sizeof(GLuint) * 8);
				fragmentList->GetTexture().BindImage(0, GL_READ_ONLY, i - 1);
				fragmentList->GetTexture().BindImage(1, GL_READ_WRITE, i);
				voxelData.radiance->GetTexture().BindImage(2, GL_READ_ONLY, i - 1, GL_TRUE, 0);
				voxelData.radiance->GetTexture().BindImage(3, GL_READ_WRITE, i, GL_TRUE, 0);

				ShaderControl->BindPipeline<VoxelMipmapCS>(GlobalShaders);
				GlobalShaders->Get<VoxelMipmapCS>()->SetLevel(i);
				glDispatchComputeIndirect(sizeof(GLuint) * 4 * (i - 1) + sizeof(GLuint));
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
			}
		}

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
	}

	void Renderer::ReadBackLightSensors()
	{
		auto shader = GlobalShaders->Get<LightSensorUpdateCS>();
		shader->UpdateValues(game->entityManager);
	}

	void Renderer::UpdateLightSensors()
	{
		RenderPhase phase("UpdateLightSensors", Timer);
		auto shader = GlobalShaders->Get<LightSensorUpdateCS>();

		GLLightData lightData[MAX_LIGHTS];
		int lightCount = FillLightData(&lightData[0], game->entityManager);

		auto sensorCollection = game->entityManager.EntitiesWith<ecs::LightSensor>();
		shader->SetSensors(sensorCollection);
		shader->SetLightData(lightCount, lightData);
		shader->SetVoxelInfo(voxelData.info);

		shader->outputTex.Clear(0);
		shader->outputTex.BindImage(0, GL_WRITE_ONLY);

		mirrorVisData.Bind(GL_SHADER_STORAGE_BUFFER, 0);
		voxelData.radiance->GetTexture().Bind(0);
		shadowMap->GetTexture().Bind(1);
		mirrorShadowMap->GetTexture().Bind(2);

		ShaderControl->BindPipeline<LightSensorUpdateCS>(GlobalShaders);
		glDispatchCompute(1, 1, 1);

		shader->StartReadback();
	}

	void Renderer::RenderPass(ecs::View &view)
	{
		RenderPhase phase("RenderPass", Timer);

		UpdateShaders();

		if (game->menuGui.RenderMode() == MenuRenderMode::Gel)
		{
			ecs::View menuView({ 1280, 1280 });
			menuView.clearMode = GL_COLOR_BUFFER_BIT;
			RenderMainMenu(menuView, true);
		}

		RenderShadowMaps();

		for (ecs::Entity ent : game->entityManager.EntitiesWith<ecs::VoxelInfo>())
		{
			voxelData.info = *ecs::UpdateVoxelInfoCache(ent, CVarVoxelGridSize.Get(), CVarVoxelSuperSample.Get());
			if (CVarUpdateVoxels.Get()) RenderVoxelGrid();
			UpdateLightSensors();
			break;
		}

		if (!mirrorSceneData)
		{
			// int count[4];
			// uint mask[SCENE_MIRROR_LIST_SIZE][MAX_MIRRORS];
			// uint list[SCENE_MIRROR_LIST_SIZE];
			// int sourceIndex[SCENE_MIRROR_LIST_SIZE];
			// mat4 reflectMat[SCENE_MIRROR_LIST_SIZE];
			// mat4 invReflectMat[SCENE_MIRROR_LIST_SIZE];
			// vec4 clipPlane[SCENE_MIRROR_LIST_SIZE];

			mirrorSceneData.Create()
			.Data(sizeof(GLint) * 4 + (sizeof(GLuint) * MAX_MIRRORS + sizeof(GLuint) * 6 + sizeof(glm::mat4) * 2) * (MAX_MIRRORS * MAX_MIRROR_RECURSION + 1 /* padding */), nullptr, GL_DYNAMIC_COPY);
		}

		mirrorSceneData.Clear(PF_R32UI, 0);

		EngineRenderTargets targets;
		targets.gBuffer0 = RTPool->Get({ PF_RGBA8, view.extents });
		targets.gBuffer1 = RTPool->Get({ PF_RGBA16F, view.extents });
		targets.gBuffer2 = RTPool->Get({ PF_RGBA16F, view.extents });
		targets.depth = RTPool->Get({ PF_DEPTH24_STENCIL8, view.extents });
		targets.shadowMap = shadowMap;
		targets.mirrorShadowMap = mirrorShadowMap;
		targets.voxelData = voxelData;
		targets.mirrorVisData = mirrorVisData;
		targets.mirrorSceneData = mirrorSceneData;
		targets.lightingGel = menuGuiTarget;

		{
			RenderPhase phase("PlayerView", Timer);

			auto mirrorIndexStencil0 = RTPool->Get({ PF_R32UI, view.extents });
			auto mirrorIndexStencil1 = RTPool->Get({ PF_R32UI, view.extents });

			const int attachmentCount = 4;

			Texture attachments[attachmentCount] =
			{
				targets.gBuffer0->GetTexture(),
				targets.gBuffer1->GetTexture(),
				targets.gBuffer2->GetTexture(),
				mirrorIndexStencil0->GetTexture(),
			};

			GLuint fb0 = RTPool->GetFramebuffer(attachmentCount, attachments, &targets.depth->GetTexture());
			attachments[attachmentCount - 1] = mirrorIndexStencil1->GetTexture();
			GLuint fb1 = RTPool->GetFramebuffer(attachmentCount, attachments, &targets.depth->GetTexture());

			glEnable(GL_CULL_FACE);
			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE);

			ecs::View forwardPassView = view;
			forwardPassView.offset = glm::ivec2();

			mirrorSceneData.Bind(GL_SHADER_STORAGE_BUFFER, 1);

			int mirrorCount = 0;
			for (auto entity : game->entityManager.EntitiesWith<ecs::Mirror>())
			{
				auto mirror = entity.Get<ecs::Mirror>();
				mirror->mirrorId = mirrorCount++;
			}

			GLMirrorData mirrorData[MAX_MIRRORS];
			int mirrorDataCount = FillMirrorData(&mirrorData[0], game->entityManager);

			auto sceneVS = GlobalShaders->Get<SceneVS>();
			auto sceneGS = GlobalShaders->Get<SceneGS>();
			auto sceneFS = GlobalShaders->Get<SceneFS>();

			int recursion = mirrorCount ? std::min(MAX_MIRROR_RECURSION, CVarMirrorRecursion.Get()) : 0;

			forwardPassView.stencil = true;
			glClearStencil(~0);
			glEnable(GL_CLIP_DISTANCE0);

			for (int bounce = 0; bounce <= recursion; bounce++)
			{
				if (bounce > 0)
				{
					RenderPhase phase("StencilCopy", Timer);

					int prevStencilBit = 1 << ((bounce - 1) % 8);
					glStencilFunc(GL_EQUAL, 0xff, ~prevStencilBit);
					glStencilMask(0);

					if (bounce % 2 == 0)
					{
						mirrorIndexStencil1->GetTexture().Bind(0);
						SetRenderTarget(&mirrorIndexStencil0->GetTexture(), nullptr);
					}
					else
					{
						mirrorIndexStencil0->GetTexture().Bind(0);
						SetRenderTarget(&mirrorIndexStencil1->GetTexture(), nullptr);
					}

					ShaderControl->BindPipeline<BasicPostVS, CopyStencilFS>(GlobalShaders);
					DrawScreenCover();
				}

				if (bounce % 2 == 0)
				{
					glBindFramebuffer(GL_FRAMEBUFFER, fb0);
					mirrorIndexStencil1->GetTexture().Bind(4);
					targets.mirrorIndexStencil = mirrorIndexStencil0;
				}
				else
				{
					glBindFramebuffer(GL_FRAMEBUFFER, fb1);
					mirrorIndexStencil0->GetTexture().Bind(4);
					targets.mirrorIndexStencil = mirrorIndexStencil1;
				}

				if (bounce == 0)
				{
					forwardPassView.clearMode |= GL_STENCIL_BUFFER_BIT;
					sceneGS->SetRenderMirrors(false);
				}
				else
				{
					{
						RenderPhase phase("MatrixGen", Timer);

						auto cs = GlobalShaders->Get<MirrorSceneCS>();
						cs->SetMirrorData(mirrorDataCount, &mirrorData[0]);

						ShaderControl->BindPipeline<MirrorSceneCS>(GlobalShaders);
						glDispatchCompute(1, 1, 1);
						glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
					}

					{
						RenderPhase phase("DepthClear", Timer);
						glDepthFunc(GL_ALWAYS);
						glDisable(GL_CULL_FACE);
						glEnable(GL_STENCIL_TEST);
						glEnable(GL_DEPTH_TEST);
						glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
						glStencilFunc(GL_EQUAL, 0xff, 0xff);
						glStencilMask(0);

						ShaderControl->BindPipeline<SceneDepthClearVS, SceneDepthClearFS>(GlobalShaders);
						DrawScreenCover();

						glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
						glDepthFunc(GL_LESS);
						glEnable(GL_CULL_FACE);
					}

					forwardPassView.clearMode = 0;
					sceneGS->SetRenderMirrors(true);
				}

				int thisStencilBit = 1 << (bounce % 8);
				glStencilFunc(GL_EQUAL, 0xff, ~thisStencilBit);
				glStencilMask(~0); // for clear
				glFrontFace(bounce % 2 == 0 ? GL_CCW : GL_CW);

				sceneFS->SetMirrorId(-1);
				sceneGS->SetParams(forwardPassView, {});

				ShaderControl->BindPipeline<SceneVS, SceneGS, SceneFS>(GlobalShaders);

				ForwardPass(forwardPassView, sceneVS, [&](ecs::Entity & ent)
				{
					if (bounce == recursion)
					{
						// Don't mark mirrors on last pass.
						glStencilMask(0);
					}
					else if (ent.Has<ecs::Mirror>())
					{
						glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
						glStencilMask(thisStencilBit);
						auto mirror = ent.Get<ecs::Mirror>();
						sceneFS->SetMirrorId(mirror->mirrorId);
					}
					else
					{
						glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
						glStencilMask(thisStencilBit);
						sceneFS->SetMirrorId(-1);
					}
				});
			}
		}

		// Run postprocessing.
		glFrontFace(GL_CCW);
		glDepthFunc(GL_LESS);
		glDisable(GL_CLIP_DISTANCE0);
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);
		glDepthMask(GL_FALSE);

		PostProcessing::Process(this, game, view, targets);

		debugGuiRenderer->Render(view);

		//AssertGLOK("Renderer::RenderFrame");
	}

	void Renderer::PrepareForView(ecs::View &view)
	{
		if (view.blend)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);

		if (view.stencil)
			glEnable(GL_STENCIL_TEST);
		else
			glDisable(GL_STENCIL_TEST);

		glDepthMask(GL_TRUE);

		glViewport(view.offset.x, view.offset.y, view.extents.x, view.extents.y);
		glScissor(view.offset.x, view.offset.y, view.extents.x, view.extents.y);

		if (view.clearMode != 0)
		{
			glClearColor(view.clearColor.r, view.clearColor.g, view.clearColor.b, view.clearColor.a);
			glClear(view.clearMode);
		}
	}

	void Renderer::ForwardPass(ecs::View &view, SceneShader *shader, const PreDrawFunc &preDraw)
	{
		RenderPhase phase("ForwardPass", Timer);
		PrepareForView(view);

		if (CVarRenderWireframe.Get())
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		for (ecs::Entity ent : game->entityManager.EntitiesWith<ecs::Renderable, ecs::Transform>())
		{
			if (ent.Has<ecs::Mirror>()) continue;
			DrawEntity(view, shader, ent, preDraw);
		}

		for (ecs::Entity ent : game->entityManager.EntitiesWith<ecs::Renderable, ecs::Transform, ecs::Mirror>())
		{
			DrawEntity(view, shader, ent, preDraw);
		}

		if (game->physics.IsDebugEnabled())
		{
			RenderPhase phase("PhysxBounds", Timer);
			MutexedVector<physx::PxDebugLine> lines =
				game->physics.GetDebugLines();
			DrawPhysxLines(view, shader, lines.Vector(), preDraw);
		}

		if (CVarRenderWireframe.Get())
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	void Renderer::DrawPhysxLines(
		ecs::View &view,
		SceneShader *shader,
		const vector<physx::PxDebugLine> &lines,
		const PreDrawFunc &preDraw)
	{
		ecs::Entity nullEnt;
		if (preDraw) preDraw(nullEnt);

		glm::vec3 viewPos = view.invViewMat * glm::vec4(0, 0, 0, 1);
		vector<SceneVertex> vertices(6*lines.size());
		for (auto &line : lines) {
			glm::vec3 lineDir = glm::normalize(glm::vec3(
				line.pos1.x - line.pos0.x,
				line.pos1.y - line.pos0.y,
				line.pos1.z - line.pos0.z));

			auto lineMid = 0.5 * (line.pos1 + line.pos0);
			glm::vec3 viewDir = glm::normalize(glm::vec3(
				viewPos.x - lineMid.x,
				viewPos.y - lineMid.y,
				viewPos.z - lineMid.z));

			const float lineWidth = 0.004f;
			glm::vec3 widthVec =
				lineWidth * glm::normalize(glm::cross(viewDir, lineDir));

			// move the positions back a bit to account for overlapping lines
			glm::vec3 pos0 = PxVec3ToGlmVec3P(line.pos0) - lineWidth * lineDir;
			glm::vec3 pos1 = PxVec3ToGlmVec3P(line.pos1) + lineWidth * lineDir;;

			auto addVertex = [&](const glm::vec3 &pos) {
				vertices.push_back({
					{pos.x, pos.y, pos.z},
					viewDir,
					{0, 0}
				});
			};

			// 2 triangles that make up a "fat" line connecting pos0 and pos1
			// with the flat face pointing at the player
			addVertex(pos0 - widthVec);
			addVertex(pos1 + widthVec);
			addVertex(pos0 + widthVec);

			addVertex(pos1 - widthVec);
			addVertex(pos1 + widthVec);
			addVertex(pos0 - widthVec);
		}

		shader->SetParams(view, glm::mat4());

		static unsigned char baseColor[4] = { 0, 0, 255, 255 };
		static BasicMaterial mat(baseColor);

		static VertexBuffer vbo;
		vbo.SetElementsVAO(vertices.size(), vertices.data(), GL_DYNAMIC_DRAW);
		vbo.BindVAO();

		mat.baseColorTex.Bind(0);
		mat.roughnessTex.Bind(1);
		mat.metallicTex.Bind(2);
		mat.heightTex.Bind(3);

		glDrawArrays(GL_TRIANGLES, 0, vbo.Elements());
	}

	void Renderer::DrawEntity(ecs::View &view, SceneShader *shader, ecs::Entity &ent, const PreDrawFunc &preDraw)
	{
		auto comp = ent.Get<ecs::Renderable>();
		if (comp->hidden)
		{
			return;
		}

		auto modelMat = ent.Get<ecs::Transform>()->GetModelTransform(*ent.GetManager());
		shader->SetParams(view, modelMat);

		if (preDraw) preDraw(ent);

		if (!comp->model->glModel)
		{
			comp->model->glModel = make_shared<GLModel>(comp->model.get());
		}
		comp->model->glModel->Draw();
	}

	void Renderer::RenderLoading(ecs::View &view)
	{
		auto screenResolution = view.extents;
		view.extents = glm::ivec2(192 / 2, 80 / 2);
		view.offset = glm::ivec2(screenResolution / 2) - view.extents / 2;

		PrepareForView(view);

		static Texture loadingTex = GAssets.LoadTexture("logos/loading.png");
		loadingTex.Bind(0);

		ShaderControl->BindPipeline<BasicPostVS, ScreenCoverFS>(GlobalShaders);
		SetDefaultRenderTarget();
		DrawScreenCover(true);
		glfwSwapBuffers(window);
	}

	void Renderer::ExpireRenderables()
	{
		int expiredCount = 0;
		for (auto &pair : renderableGCQueue)
		{
			if (pair.second-- < 0) expiredCount++;
		}
		while (expiredCount-- > 0)
		{
			renderableGCQueue.pop_front();
		}
	}

	void Renderer::BeginFrame()
	{
		ExpireRenderables();
		ReadBackLightSensors();
	}

	void Renderer::EndFrame()
	{
		RTPool->TickFrame();
		glfwSwapBuffers(window);
	}

	void Renderer::SetRenderTargets(size_t attachmentCount, const Texture *attachments, const Texture *depth)
	{
		GLuint fb = RTPool->GetFramebuffer(attachmentCount, attachments, depth);
		glBindFramebuffer(GL_FRAMEBUFFER, fb);
	}

	void Renderer::SetRenderTarget(const Texture *attachment0, const Texture *depth)
	{
		SetRenderTargets(1, attachment0, depth);
	}

	void Renderer::SetDefaultRenderTarget()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
}
