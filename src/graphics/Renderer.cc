#include "graphics/Renderer.hh"
#include "graphics/RenderTargetPool.hh"
#include "graphics/Shader.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/Util.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/SceneShaders.hh"
#include "graphics/GPUTimer.hh"
#include "graphics/postprocess/PostProcess.hh"
#include "core/Game.hh"
#include "core/Logging.hh"
#include "core/CVar.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/View.hh"
#include "ecs/components/Light.hh"

namespace sp
{
	class ExampleCS : public Shader
	{
		SHADER_TYPE(ExampleCS)
		using Shader::Shader;
	};

	IMPLEMENT_SHADER_TYPE(ExampleCS, "compute_example.glsl", Compute);

	Renderer::Renderer(Game *game) : GraphicsContext(game)
	{
	}

	Renderer::~Renderer()
	{
		if (ShaderControl)
			delete ShaderControl;
	}

	struct DefaultMaterial
	{
		Texture baseColorTex, roughnessTex, bumpTex;

		DefaultMaterial()
		{
			unsigned char baseColor[4] = { 255, 255, 255, 255 };
			unsigned char roughness[4] = { 200, 200, 200, 255 };
			unsigned char bump[4] = { 127, 127, 127, 255 };

			baseColorTex.Create()
			.Filter(GL_NEAREST, GL_NEAREST).Wrap(GL_REPEAT, GL_REPEAT)
			.Size(1, 1).Storage(PF_RGB8).Image2D(baseColor);

			roughnessTex.Create()
			.Filter(GL_NEAREST, GL_NEAREST).Wrap(GL_REPEAT, GL_REPEAT)
			.Size(1, 1).Storage(PF_RGB8).Image2D(roughness);

			bumpTex.Create()
			.Filter(GL_NEAREST, GL_NEAREST).Wrap(GL_REPEAT, GL_REPEAT)
			.Size(1, 1).Storage(PF_RGB8).Image2D(bump);
		}
	};

	static CVar<float> CVarFlashlightIntensity("r.Flashlight", 2000, "Flashlight intensity");
	static CVar<bool> CVarRenderWireframe("r.Wireframe", false, "Render wireframes");

	// TODO Clean up Renderable when unloaded.
	void PrepareRenderable(ecs::Handle<ecs::Renderable> comp)
	{
		static DefaultMaterial defaultMat;

		for (auto primitive : comp->model->primitives)
		{
			primitive->indexBufferHandle = comp->model->LoadBuffer(primitive->indexBuffer.bufferName);

			if (primitive->baseColorName.length() > 0)
				primitive->baseColorTex = comp->model->LoadTexture(primitive->baseColorName);
			else
				primitive->baseColorTex = &defaultMat.baseColorTex;

			if (primitive->roughnessName.length() > 0)
				primitive->roughnessTex = comp->model->LoadTexture(primitive->roughnessName);
			else
				primitive->roughnessTex = &defaultMat.roughnessTex;

			if (primitive->bumpName.length() > 0)
				primitive->bumpTex = comp->model->LoadTexture(primitive->bumpName);
			else
				primitive->bumpTex = &defaultMat.bumpTex;

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

			if (primitive->bumpTex)
				primitive->bumpTex->Bind(2);

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
		glEnable(GL_FRAMEBUFFER_SRGB);

		RTPool = new RenderTargetPool();

		ShaderControl = new ShaderManager();
		ShaderControl->CompileAll(GlobalShaders);

		timer = new GPUTimer();

		for (ecs::Entity ent : game->entityManager.EntitiesWith<ecs::Renderable>())
		{
			auto comp = ent.Get<ecs::Renderable>();
			PrepareRenderable(comp);
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

	void Renderer::RenderShadowMaps()
	{
		RenderPhase phase("ShadowMaps", timer);

		// TODO(xthexder): Handle lights without shadowmaps
		glm::ivec2 renderTargetSize;
		bool first = true;
		for (auto entity : game->entityManager.EntitiesWith<ecs::Light>())
		{
			auto light = entity.Get<ecs::Light>();
			if (first)
			{
				light->intensity = CVarFlashlightIntensity.Get();
				first = false;
			}
			if (entity.Has<ecs::View>())
			{
				auto view = updateLightCaches(entity, light);
				light->mapOffset = glm::vec4(renderTargetSize.x, 0, view->extents.x, view->extents.y);
				view->offset = glm::ivec2(light->mapOffset);
				view->clearMode = 0;

				renderTargetSize.x += view->extents.x;
				if (view->extents.y > renderTargetSize.y)
					renderTargetSize.y = view->extents.y;
			}
		}

		if (first) return;

		if (!shadowMap || glm::ivec2(shadowMap->GetDesc().extent) != renderTargetSize)
		{
			shadowMap = RTPool->Get(RenderTargetDesc(PF_R32F, renderTargetSize));
		}

		auto depthTarget = RTPool->Get(RenderTargetDesc(PF_DEPTH32F, renderTargetSize));
		SetRenderTarget(&shadowMap->GetTexture(), &depthTarget->GetTexture());

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
				ForwardPass(*view, shadowMapVS);
			}
		}
	}

	const int VoxelGridSize = 256;
	const int VoxelMipLevels = 3;
	const float VoxelSize = 0.08;
	const glm::vec3 VoxelGridCenter = glm::vec3(0, 5, 0);
	const int VoxelListSize = VoxelGridSize * VoxelGridSize * VoxelGridSize / 8;

	void Renderer::PrepareVoxelTextures()
	{
		glm::ivec3 renderTargetSize = glm::ivec3(VoxelGridSize * 2, VoxelGridSize, VoxelGridSize);

		if (!voxelData.fragmentList || voxelData.fragmentList->GetDesc().extent != glm::ivec3(VoxelListSize, 1, 1)) {
			voxelData.fragmentList = RTPool->Get(RenderTargetDesc(PF_R32UI, glm::ivec3(VoxelListSize, 1, 1)));
			uint32_t data = 0;
			voxelData.fragmentList->GetTexture().Image2D(&data, 0, 1, 1);
		}

		if (!voxelData.packedColor || voxelData.packedColor->GetDesc().extent != renderTargetSize) {
			voxelData.packedColor = RTPool->Get(RenderTargetDesc(PF_R32UI, renderTargetSize));
			voxelData.packedColor->GetTexture().Clear(0);
		}
		if (!voxelData.packedNormal || voxelData.packedNormal->GetDesc().extent != renderTargetSize) {
			voxelData.packedNormal = RTPool->Get(RenderTargetDesc(PF_R32UI, renderTargetSize));
			voxelData.packedNormal->GetTexture().Clear(0);
		}
		if (!voxelData.packedRadiance || voxelData.packedRadiance->GetDesc().extent != renderTargetSize) {
			voxelData.packedRadiance = RTPool->Get(RenderTargetDesc(PF_R32UI, renderTargetSize));
			voxelData.packedRadiance->GetTexture().Clear(0);
		}

		RenderTargetDesc unpackedDesc(PF_RGBA8, renderTargetSize);
		unpackedDesc.levels = VoxelMipLevels;
		if (!voxelData.color || voxelData.color->GetDesc() != unpackedDesc)
		{
			voxelData.color = RTPool->Get(unpackedDesc);

			for (uint32 i = 0; i < VoxelMipLevels; i++)
			{
				voxelData.color->GetTexture().Clear(0, i);
			}
		}
		if (!voxelData.normal || voxelData.normal->GetDesc() != unpackedDesc)
		{
			voxelData.normal = RTPool->Get(unpackedDesc);

			for (uint32 i = 0; i < VoxelMipLevels; i++)
			{
				voxelData.normal->GetTexture().Clear(0, i);
			}
		}
		if (!voxelData.radiance || voxelData.radiance->GetDesc() != unpackedDesc)
		{
			voxelData.radiance = RTPool->Get(unpackedDesc);

			for (uint32 i = 0; i < VoxelMipLevels; i++)
			{
				voxelData.radiance->GetTexture().Clear(0, i);
			}
		}
	}

	void Renderer::RenderVoxelGrid()
	{
		RenderPhase phase("VoxelGrid", timer);

		PrepareVoxelTextures();

		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

		ecs::View ortho;
		ortho.viewMat = glm::scale(glm::mat4(), glm::vec3(2.0 / (VoxelGridSize * VoxelSize)));
		ortho.viewMat = glm::translate(ortho.viewMat, -VoxelGridCenter);
		ortho.projMat = glm::mat4();
		ortho.offset = glm::ivec2(0);
		ortho.extents = glm::ivec2(VoxelGridSize);
		ortho.clearMode = 0;

		auto voxelVS = GlobalShaders->Get<VoxelRasterVS>();
		glViewport(0, 0, VoxelGridSize, VoxelGridSize);

		{
			RenderPhase phase("Clear", timer);

			// TODO(xthexder): Remove me
			for (uint32 i = 0; i < VoxelMipLevels; i++)
			{
				voxelData.color->GetTexture().Clear(0, i);
				voxelData.normal->GetTexture().Clear(0, i);
				voxelData.radiance->GetTexture().Clear(0, i);
			}
		}

		{
			RenderPhase phase("Accumulate", timer);
			voxelData.fragmentList->GetTexture().BindImage(0, GL_READ_WRITE, 0, GL_TRUE, 0);
			voxelData.packedColor->GetTexture().BindImage(1, GL_READ_WRITE, 0, GL_TRUE, 0);
			voxelData.packedNormal->GetTexture().BindImage(2, GL_READ_WRITE, 0, GL_TRUE, 0);

			ShaderControl->BindPipeline<VoxelRasterVS, VoxelRasterGS, VoxelRasterFS>(GlobalShaders);
			ForwardPass(ortho, voxelVS);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		}

		{
			RenderPhase phase("Convert", timer);
			voxelData.fragmentList->GetTexture().BindImage(0, GL_READ_WRITE, 0, GL_TRUE, 0);
			voxelData.packedColor->GetTexture().BindImage(1, GL_READ_WRITE, 0, GL_TRUE, 0);
			voxelData.packedNormal->GetTexture().BindImage(2, GL_READ_WRITE, 0, GL_TRUE, 0);

			ShaderControl->BindPipeline<VoxelRasterVS, VoxelRasterGS, VoxelConvertFS>(GlobalShaders);
			auto voxelConvertFS = GlobalShaders->Get<VoxelConvertFS>();
			voxelConvertFS->SetLevel(0);
			voxelData.color->GetTexture().BindImage(3, GL_READ_WRITE, 0, GL_TRUE, 0);
			voxelData.normal->GetTexture().BindImage(4, GL_READ_WRITE, 0, GL_TRUE, 0);

			ForwardPass(ortho, voxelVS);
			glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		}

		{
			// RenderPhase phase("Mipmap", timer);
			// voxelData.color->GetTexture().GenMipmap();
		}

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
	}

	void Renderer::RenderPass(ecs::View &view)
	{
		RenderPhase phase("RenderPass", timer);

		EngineRenderTargets targets;
		targets.gBuffer0 = RTPool->Get({ PF_RGBA8, view.extents });
		targets.gBuffer1 = RTPool->Get({ PF_RGBA16F, view.extents });
		targets.depth = RTPool->Get({ PF_DEPTH32F, view.extents });
		targets.shadowMap = shadowMap;
		targets.voxelData = voxelData;

		Texture attachments[] =
		{
			targets.gBuffer0->GetTexture(),
			targets.gBuffer1->GetTexture(),
		};

		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		SetRenderTargets(2, attachments, &targets.depth->GetTexture());

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

	void Renderer::ForwardPass(ecs::View &view, SceneShader *shader)
	{
		RenderPhase phase("ForwardPass", timer);
		PrepareForView(view);

		if (CVarRenderWireframe.Get())
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		for (ecs::Entity ent : game->entityManager.EntitiesWith<ecs::Renderable, ecs::Transform>())
		{
			auto comp = ent.Get<ecs::Renderable>();
			auto modelMat = ent.Get<ecs::Transform>()->GetModelTransform(*ent.GetManager());
			shader->SetParams(view, modelMat);
			DrawRenderable(comp);
		}

		if (CVarRenderWireframe.Get())
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	void Renderer::BeginFrame(ecs::View &view, int fullscreen)
	{
		if (prevFullscreen != fullscreen)
		{
			if (fullscreen == 0)
			{
				glfwSetWindowMonitor(window, nullptr, prevWindowPos.x, prevWindowPos.y, view.extents.x, view.extents.y, 0);
			}
			else if (fullscreen == 1)
			{
				glfwGetWindowPos(window, &prevWindowPos.x, &prevWindowPos.y);
				glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, view.extents.x, view.extents.y, 60);
			}
		}

		if (prevWindowSize != view.extents)
		{
			glfwSetWindowSize(window, view.extents.x, view.extents.y);
		}

		prevFullscreen = fullscreen;
		prevWindowSize = view.extents;
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
