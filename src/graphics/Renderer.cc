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
#include "core/Game.hh"
#include "core/Logging.hh"
#include "core/CVar.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/View.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/VoxelInfo.hh"
#include "ecs/components/Mirror.hh"

#include <glm/gtx/component_wise.hpp>
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
	}

	struct DefaultMaterial
	{
		Texture baseColorTex, roughnessTex, metallicTex, heightTex;

		DefaultMaterial()
		{
			unsigned char baseColor[4] = { 255, 255, 255, 255 };
			unsigned char roughness[4] = { 255, 255, 255, 255 };
			unsigned char metallic[4] = { 0, 0, 0, 0 };
			unsigned char bump[4] = { 127, 127, 127, 255 };

			baseColorTex.Create()
			.Filter(GL_NEAREST, GL_NEAREST).Wrap(GL_REPEAT, GL_REPEAT)
			.Size(1, 1).Storage(PF_RGB8).Image2D(baseColor);

			roughnessTex.Create()
			.Filter(GL_NEAREST, GL_NEAREST).Wrap(GL_REPEAT, GL_REPEAT)
			.Size(1, 1).Storage(PF_R8).Image2D(roughness);

			metallicTex.Create()
			.Filter(GL_NEAREST, GL_NEAREST).Wrap(GL_REPEAT, GL_REPEAT)
			.Size(1, 1).Storage(PF_R8).Image2D(metallic);

			heightTex.Create()
			.Filter(GL_NEAREST, GL_NEAREST).Wrap(GL_REPEAT, GL_REPEAT)
			.Size(1, 1).Storage(PF_R8).Image2D(bump);
		}
	};

	static CVar<bool> CVarRenderWireframe("r.Wireframe", false, "Render wireframes");
	static CVar<int> CVarMirrorRecursion("r.MirrorRecursion", 2, "Mirror recursion depth");

	// TODO(xthexder) Clean up Renderable when unloaded.
	void PrepareRenderable(ecs::Handle<ecs::Renderable> comp)
	{
		static DefaultMaterial defaultMat;

		for (auto primitive : comp->model->primitives)
		{
			primitive->indexBufferHandle = comp->model->LoadBuffer(primitive->indexBuffer.bufferName);

			primitive->baseColorTex = comp->model->LoadTexture(primitive->materialName, "baseColor");
			primitive->roughnessTex = comp->model->LoadTexture(primitive->materialName, "roughness");
			primitive->metallicTex = comp->model->LoadTexture(primitive->materialName, "metallic");
			primitive->heightTex = comp->model->LoadTexture(primitive->materialName, "height");

			if (!primitive->baseColorTex) primitive->baseColorTex = &defaultMat.baseColorTex;
			if (!primitive->roughnessTex) primitive->roughnessTex = &defaultMat.roughnessTex;
			if (!primitive->metallicTex) primitive->metallicTex = &defaultMat.metallicTex;
			if (!primitive->heightTex) primitive->heightTex = &defaultMat.heightTex;

			glCreateVertexArrays(1, &primitive->vertexBufferHandle);
			for (int i = 0; i < 3; i++)
			{
				auto *attr = &primitive->attributes[i];
				if (attr->componentCount == 0) continue;
				glEnableVertexArrayAttrib(primitive->vertexBufferHandle, i);
				glVertexArrayAttribFormat(primitive->vertexBufferHandle, i, attr->componentCount, attr->componentType, GL_FALSE, 0);
				glVertexArrayVertexBuffer(primitive->vertexBufferHandle, i, comp->model->LoadBuffer(attr->bufferName), attr->byteOffset, attr->byteStride);
			}
		}
	}

	void DrawRenderable(ecs::Handle<ecs::Renderable> comp)
	{
		for (auto primitive : comp->model->primitives)
		{
			glBindVertexArray(primitive->vertexBufferHandle);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, primitive->indexBufferHandle);

			if (primitive->baseColorTex)
				primitive->baseColorTex->Bind(0);

			if (primitive->roughnessTex)
				primitive->roughnessTex->Bind(1);

			if (primitive->metallicTex)
				primitive->metallicTex->Bind(2);

			if (primitive->heightTex)
				primitive->heightTex->Bind(3);

			glDrawElements(
				primitive->drawMode,
				primitive->indexBuffer.components,
				primitive->indexBuffer.componentType,
				(char *) primitive->indexBuffer.byteOffset
			);
		}
	}

	void Renderer::Prepare()
	{
		Assert(GLEW_ARB_compute_shader, "ARB_compute_shader required");
		Assert(GLEW_ARB_direct_state_access, "ARB_direct_state_access required");
		Assert(GLEW_ARB_multi_bind, "ARB_multi_bind required");

		glEnable(GL_FRAMEBUFFER_SRGB);

		RTPool = new RenderTargetPool();

		int voxelGridSize = game->options["voxel-gridsize"].as<int>();
		float voxelSuperSampleScale = game->options["voxel-supersample"].as<float>();

		ShaderControl = new ShaderManager();
		ShaderManager::SetDefine("VoxelGridSize", std::to_string(voxelGridSize));
		ShaderManager::SetDefine("VoxelSuperSampleScale", std::to_string(voxelSuperSampleScale));
		ShaderControl->CompileAll(GlobalShaders);

		int mirrorCount = 0;
		for (ecs::Entity ent : game->entityManager.EntitiesWith<ecs::Renderable>())
		{
			auto comp = ent.Get<ecs::Renderable>();
			PrepareRenderable(comp);

			if (ent.Has<ecs::Mirror>())
			{
				auto mirror = ent.Get<ecs::Mirror>();
				mirror->mirrorId = mirrorCount++;
			}
		}

		voxelInfo = {voxelGridSize, 0.1f, voxelSuperSampleScale, glm::vec3(0), glm::vec3(0), glm::vec3(0)};
		for (ecs::Entity ent : game->entityManager.EntitiesWith<ecs::VoxelInfo>())
		{
			voxelInfo = *ent.Get<ecs::VoxelInfo>();
			if (!voxelInfo.gridSize) voxelInfo.gridSize = voxelGridSize;
			if (!voxelInfo.superSampleScale) voxelInfo.superSampleScale = voxelSuperSampleScale;
			voxelInfo.voxelGridCenter = (voxelInfo.gridMin + voxelInfo.gridMax) * glm::vec3(0.5);
			voxelInfo.voxelSize = glm::compMax(voxelInfo.gridMax - voxelInfo.gridMin) / voxelInfo.gridSize;
			break;
		}

		AssertGLOK("Renderer::Prepare");
	}

	ecs::Handle<ecs::View> updateLightCaches(ecs::Entity entity, ecs::Handle<ecs::Light> light)
	{
		auto view = entity.Get<ecs::View>();

		view->aspect = (float)view->extents.x / (float)view->extents.y;
		view->projMat = glm::perspective(light->spotAngle * 2.0f, view->aspect, view->clip[0], view->clip[1]);
		view->invProjMat = glm::inverse(view->projMat);

		auto transform = entity.Get<ecs::Transform>();
		view->invViewMat = transform->GetModelTransform(*entity.GetManager());
		view->viewMat = glm::inverse(view->invViewMat);

		return view;
	}

	const int MirrorShadowMapResolution = 512;

	void Renderer::RenderShadowMaps()
	{
		RenderPhase phase("ShadowMaps", Timer);

		// TODO(xthexder): Handle lights without shadowmaps
		glm::ivec2 renderTargetSize;
		int lightCount = 0;
		for (auto entity : game->entityManager.EntitiesWith<ecs::Light>())
		{
			auto light = entity.Get<ecs::Light>();
			if (entity.Has<ecs::View>())
			{
				auto view = updateLightCaches(entity, light);
				light->mapOffset = glm::vec4(renderTargetSize.x, 0, view->extents.x, view->extents.y);
				light->lightId = lightCount++;
				view->offset = glm::ivec2(light->mapOffset);
				view->clearMode = 0;

				renderTargetSize.x += view->extents.x;
				if (view->extents.y > renderTargetSize.y)
					renderTargetSize.y = view->extents.y;
			}
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
			// uint maskL[MAX_LIGHTS];
			// uint maskM[MAX_MIRRORS];
			// uint list[MAX_LIGHTS * MAX_MIRRORS];
			// int sourceLight[MAX_LIGHTS * MAX_MIRRORS];
			// int sourceIndex[MAX_LIGHTS * MAX_MIRRORS];
			// mat4 viewMat[MAX_LIGHTS * MAX_MIRRORS];
			// mat4 invViewMat[MAX_LIGHTS * MAX_MIRRORS];
			// mat4 projMat[MAX_LIGHTS * MAX_MIRRORS];
			// mat4 invProjMat[MAX_LIGHTS * MAX_MIRRORS];
			// vec2 clip[MAX_LIGHTS * MAX_MIRRORS];
			// vec4 nearInfo[MAX_LIGHTS * MAX_MIRRORS];
			// vec3 lightDirection[MAX_LIGHTS * MAX_MIRRORS]; // stride of vec4

			mirrorVisData.Create()
			.Data(sizeof(GLint) * 4 + (sizeof(GLuint) * 14 + sizeof(glm::mat4) * 4) * (MAX_LIGHTS * MAX_MIRRORS + 1 /* padding */), nullptr, GL_DYNAMIC_COPY);
		}

		// TODO(xthexder): Try 16 bit depth
		auto depthTarget = RTPool->Get(RenderTargetDesc(PF_DEPTH32F, renderTargetSize));
		SetRenderTarget(&shadowMap->GetTexture(), &depthTarget->GetTexture());

		mirrorVisData.Clear(PF_R32UI, 0);
		mirrorVisData.Bind(GL_SHADER_STORAGE_BUFFER, 0);

		glViewport(0, 0, renderTargetSize.x, renderTargetSize.y);
		glDisable(GL_SCISSOR_TEST);
		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glClear(GL_DEPTH_BUFFER_BIT);

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

		GLLightData lightData[MAX_LIGHTS];
		int lightDataCount = FillLightData(&lightData[0], game->entityManager);
		int recursion = CVarMirrorRecursion.Get();

		for (int bounce = 0; bounce < recursion; bounce++)
		{
			{
				RenderPhase phase("MatrixGen", Timer);

				auto mirrorMapCS = GlobalShaders->Get<MirrorMapCS>();

				GLMirrorData mirrorData[MAX_MIRRORS];
				int mirrorCount = FillMirrorData(&mirrorData[0], game->entityManager);
				mirrorMapCS->SetLightData(lightDataCount, &lightData[0]);
				mirrorMapCS->SetMirrorData(mirrorCount, &mirrorData[0]);

				ShaderControl->BindPipeline<MirrorMapCS>(GlobalShaders);
				glDispatchCompute(1, 1, 1);
				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			}

			{
				RenderPhase phase("MirrorMaps", Timer);

				// TODO(xthexder): Set Z size properly
				auto mirrorMapResolution = glm::ivec3(MirrorShadowMapResolution, MirrorShadowMapResolution, 8);
				RenderTargetDesc mirrorMapDesc(PF_R32F, mirrorMapResolution);
				mirrorMapDesc.textureArray = true;
				if (!mirrorShadowMap || mirrorShadowMap->GetDesc() != mirrorMapDesc)
				{
					mirrorShadowMap = RTPool->Get(mirrorMapDesc);
				}

				RenderTargetDesc depthDesc(PF_DEPTH32F, mirrorMapResolution);
				depthDesc.textureArray = true;
				auto depthTarget = RTPool->Get(depthDesc);
				SetRenderTarget(&mirrorShadowMap->GetTexture(), &depthTarget->GetTexture());

				ecs::View basicView;
				basicView.offset = glm::ivec2(0);
				basicView.extents = glm::ivec2(MirrorShadowMapResolution);

				if (bounce > 0)
					basicView.clearMode = 0;

				ShaderControl->BindPipeline<MirrorMapVS, MirrorMapGS, MirrorMapFS>(GlobalShaders);

				auto mirrorMapFS = GlobalShaders->Get<MirrorMapFS>();
				auto mirrorMapVS = GlobalShaders->Get<MirrorMapVS>();

				mirrorMapFS->SetLightData(lightDataCount, &lightData[0]);
				shadowMap->GetTexture().Bind(4);
				mirrorMapFS->SetMirrorId(-1);

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
		auto VoxelGridSize = voxelInfo.gridSize;
		glm::ivec3 packedSize = glm::ivec3(VoxelGridSize * 6, VoxelGridSize, VoxelGridSize);
		glm::ivec3 unpackedSize = glm::ivec3(VoxelGridSize, VoxelGridSize, VoxelGridSize);
		auto VoxelMipLevels = ceil(log2(VoxelGridSize));

		if (!computeIndirectBuffer)
		{
			computeIndirectBuffer.Create()
			.Data(sizeof(GLuint) * 4 * VoxelMipLevels, nullptr, GL_DYNAMIC_COPY);
		}

		auto VoxelListSize = VoxelGridSize * VoxelGridSize * VoxelGridSize / 4;
		RenderTargetDesc listDesc(PF_R32UI, glm::ivec2(8192, ceil(VoxelListSize / 8192.0)));
		listDesc.levels = VoxelMipLevels;
		if (!voxelData.fragmentList || voxelData.fragmentList->GetDesc() != listDesc)
		{
			voxelData.fragmentList = RTPool->Get(listDesc);
		}

		if (!voxelData.packedData || voxelData.packedData->GetDesc().extent != packedSize)
		{
			voxelData.packedData = RTPool->Get(RenderTargetDesc(PF_R32UI, packedSize));
			voxelData.packedData->GetTexture().Clear(0);
		}
		if (!voxelData.color || voxelData.color->GetDesc().extent != unpackedSize)
		{
			voxelData.color = RTPool->Get(RenderTargetDesc(PF_RGBA8, unpackedSize));
			voxelData.color->GetTexture().Clear(0);
		}
		if (!voxelData.normal || voxelData.normal->GetDesc().extent != unpackedSize)
		{
			voxelData.normal = RTPool->Get(RenderTargetDesc(PF_RGBA16F, unpackedSize));
			voxelData.normal->GetTexture().Clear(0);
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

		auto VoxelGridSize = voxelInfo.gridSize;

		ecs::View ortho;
		ortho.viewMat = glm::scale(glm::mat4(), glm::vec3(2.0 / (VoxelGridSize * voxelInfo.voxelSize)));
		ortho.viewMat = glm::translate(ortho.viewMat, -voxelInfo.voxelGridCenter);
		ortho.projMat = glm::mat4();
		ortho.extents = glm::ivec2(VoxelGridSize * voxelInfo.superSampleScale);
		ortho.clearMode = 0;

		auto renderTarget = RTPool->Get(RenderTargetDesc(PF_R8, ortho.extents));
		SetRenderTarget(&renderTarget->GetTexture(), nullptr);

		auto voxelVS = GlobalShaders->Get<VoxelRasterVS>();

		GLuint listData[] = {0, 0, 1, 1};
		computeIndirectBuffer.Clear(PF_RGBA32UI, listData);

		{
			RenderPhase phase("Accumulate", Timer);
			computeIndirectBuffer.Bind(GL_ATOMIC_COUNTER_BUFFER, 0);
			voxelData.fragmentList->GetTexture().BindImage(0, GL_WRITE_ONLY, 0);
			voxelData.packedData->GetTexture().BindImage(1, GL_READ_WRITE, 0, GL_TRUE, 0);

			GLLightData lightData[MAX_LIGHTS];
			int lightCount = FillLightData(&lightData[0], game->entityManager);

			GlobalShaders->Get<VoxelRasterFS>()->SetLightData(lightCount, &lightData[0]);
			GlobalShaders->Get<VoxelRasterFS>()->SetVoxelInfo(voxelInfo);
			shadowMap->GetTexture().Bind(4);
			mirrorShadowMap->GetTexture().Bind(5);
			mirrorVisData.Bind(GL_SHADER_STORAGE_BUFFER, 0);

			ShaderControl->BindPipeline<VoxelRasterVS, VoxelRasterGS, VoxelRasterFS>(GlobalShaders);
			ForwardPass(ortho, voxelVS);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
		}

		computeIndirectBuffer.Bind(GL_DISPATCH_INDIRECT_BUFFER);

		{
			RenderPhase phase("Convert", Timer);
			computeIndirectBuffer.Bind(GL_ATOMIC_COUNTER_BUFFER, 0);
			voxelData.fragmentList->GetTexture().BindImage(0, GL_READ_ONLY, 0);
			voxelData.packedData->GetTexture().BindImage(1, GL_READ_WRITE, 0, GL_TRUE, 0);
			voxelData.color->GetTexture().BindImage(2, GL_WRITE_ONLY, 0, GL_TRUE, 0);
			voxelData.normal->GetTexture().BindImage(3, GL_WRITE_ONLY, 0, GL_TRUE, 0);
			voxelData.radiance->GetTexture().BindImage(4, GL_WRITE_ONLY, 0, GL_TRUE, 0);

			ShaderControl->BindPipeline<VoxelConvertCS>(GlobalShaders);
			glDispatchComputeIndirect(sizeof(GLuint));
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
		}
		{
			RenderPhase phase("Mipmap", Timer);
			for (uint32 i = 1; i < voxelData.radiance->GetDesc().levels; i++)
			{
				computeIndirectBuffer.Bind(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 4 * (i - 1), sizeof(GLuint) * 8);
				voxelData.fragmentList->GetTexture().BindImage(0, GL_READ_ONLY, i - 1);
				voxelData.fragmentList->GetTexture().BindImage(1, GL_READ_WRITE, i);
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

	void Renderer::ClearVoxelGrid()
	{
		RenderPhase phase("VoxelClear", Timer);

		computeIndirectBuffer.Bind(GL_DISPATCH_INDIRECT_BUFFER);

		for (uint32 i = 0; i < voxelData.radiance->GetDesc().levels; i++)
		{
			computeIndirectBuffer.Bind(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 4 * i, sizeof(GLuint));
			voxelData.fragmentList->GetTexture().BindImage(0, GL_READ_ONLY, i);
			voxelData.color->GetTexture().BindImage(1, GL_WRITE_ONLY, i, GL_TRUE, 0);
			voxelData.normal->GetTexture().BindImage(2, GL_WRITE_ONLY, i, GL_TRUE, 0);
			voxelData.radiance->GetTexture().BindImage(3, GL_WRITE_ONLY, i, GL_TRUE, 0);

			ShaderControl->BindPipeline<VoxelClearCS>(GlobalShaders);
			GlobalShaders->Get<VoxelClearCS>()->SetLevel(i);
			glDispatchComputeIndirect(sizeof(GLuint) * 4 * i + sizeof(GLuint));
		}

		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	}

	void Renderer::UpdateLightSensors()
	{
		RenderPhase phase("UpdateLightSensors", Timer);
		auto shader = GlobalShaders->Get<LightSensorUpdateCS>();

		GLLightData lightData[MAX_LIGHTS];
		int lightCount = FillLightData(&lightData[0], game->entityManager);

		auto sensorCollection = game->entityManager.EntitiesWith<ecs::LightSensor>();
		shader->UpdateValues(game->entityManager);
		shader->SetSensors(sensorCollection);
		shader->SetLightData(lightCount, lightData);
		shader->SetVoxelInfo(voxelInfo);

		shader->outputTex.Clear(0);
		shader->outputTex.BindImage(0, GL_WRITE_ONLY);

		voxelData.radiance->GetTexture().Bind(0);
		shadowMap->GetTexture().Bind(1);

		ShaderControl->BindPipeline<LightSensorUpdateCS>(GlobalShaders);
		glDispatchCompute(1, 1, 1);

		shader->StartReadback();
	}

	void Renderer::RenderPass(ecs::View &view)
	{
		RenderPhase phase("RenderPass", Timer);

		RenderShadowMaps();
		RenderVoxelGrid();
		UpdateLightSensors();

		EngineRenderTargets targets;
		targets.gBuffer0 = RTPool->Get({ PF_RGBA8, view.extents });
		targets.gBuffer1 = RTPool->Get({ PF_RGBA16F, view.extents });
		targets.gBuffer2 = RTPool->Get({ PF_RGBA16F, view.extents });
		targets.depth = RTPool->Get({ PF_DEPTH32F, view.extents });
		targets.shadowMap = shadowMap;
		targets.mirrorShadowMap = mirrorShadowMap;
		targets.voxelData = voxelData;

		Texture attachments[] =
		{
			targets.gBuffer0->GetTexture(),
			targets.gBuffer1->GetTexture(),
			targets.gBuffer2->GetTexture(),
		};

		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		SetRenderTargets(3, attachments, &targets.depth->GetTexture());

		ecs::View forwardPassView = view;
		forwardPassView.offset = glm::ivec2();

		ShaderControl->BindPipeline<SceneVS, SceneGS, SceneFS>(GlobalShaders);

		auto sceneVS = GlobalShaders->Get<SceneVS>();
		ForwardPass(forwardPassView, sceneVS);

		// Run postprocessing.
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		PostProcessing::Process(this, game, view, targets);

		//AssertGLOK("Renderer::RenderFrame");
	}

	void Renderer::PrepareForView(ecs::View &view)
	{
		glDisable(GL_BLEND);
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

	void Renderer::ForwardPass(ecs::View &view, SceneShader *shader, std::function<void(ecs::Entity &)> preDraw)
	{
		RenderPhase phase("ForwardPass", Timer);
		PrepareForView(view);

		if (CVarRenderWireframe.Get())
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		for (ecs::Entity ent : game->entityManager.EntitiesWith<ecs::Renderable, ecs::Transform>())
		{
			auto comp = ent.Get<ecs::Renderable>();
			auto modelMat = ent.Get<ecs::Transform>()->GetModelTransform(*ent.GetManager());
			shader->SetParams(view, modelMat);

			preDraw(ent);
			DrawRenderable(comp);
		}

		if (CVarRenderWireframe.Get())
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	void Renderer::BeginFrame(ecs::View &view, int fullscreen)
	{
		ResizeWindow(view, fullscreen);
	}

	void Renderer::EndFrame()
	{
		ClearVoxelGrid();
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
