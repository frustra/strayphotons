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

	shared_ptr<RenderTarget> Renderer::RenderShadowMaps()
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

		if (first)
		{
			return nullptr;
		}

		auto renderTarget = RTPool->Get(RenderTargetDesc(PF_R32F, renderTargetSize));
		auto depthTarget = RTPool->Get(RenderTargetDesc(PF_DEPTH32F, renderTargetSize));
		SetRenderTarget(&renderTarget->GetTexture(), &depthTarget->GetTexture());

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

		return renderTarget;
	}

	const int VoxelGridSize = 256;
	const float VoxelSize = 0.0453;
	const glm::vec3 VoxelGridCenter = glm::vec3(0, 5, 0);

	VoxelColors Renderer::RenderVoxelGrid()
	{
		RenderPhase phase("VoxelGrid", timer);

		glm::ivec3 renderTargetSize = glm::ivec3(VoxelGridSize * 2, VoxelGridSize, VoxelGridSize);

		auto packedVoxels = RTPool->Get(RenderTargetDesc(PF_R32UI, renderTargetSize));
		auto unpackedVoxels = RTPool->Get(RenderTargetDesc(PF_RGBA8, renderTargetSize));

		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

		glClearTexImage(packedVoxels->GetTexture().handle, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
		glClearTexImage(unpackedVoxels->GetTexture().handle, 0, GL_RGBA, GL_UNSIGNED_INT, nullptr);

		ecs::View ortho;
		ortho.viewMat = glm::scale(glm::mat4(), glm::vec3(2.0 / (VoxelGridSize * VoxelSize)));
		ortho.viewMat = glm::translate(ortho.viewMat, -VoxelGridCenter);
		ortho.projMat = glm::mat4();
		ortho.offset = glm::ivec2(0);
		ortho.extents = glm::ivec2(VoxelGridSize);
		ortho.clearMode = 0;

		{
			glViewport(0, 0, VoxelGridSize, VoxelGridSize);
			auto voxelVS = GlobalShaders->Get<VoxelRasterVS>();

			packedVoxels->GetTexture().BindImage(0, GL_READ_WRITE, 0, GL_TRUE, 0);
			unpackedVoxels->GetTexture().BindImage(1, GL_READ_WRITE, 0, GL_TRUE, 0);

			ShaderControl->BindPipeline<VoxelRasterVS, VoxelRasterGS, VoxelRasterFS>(GlobalShaders);
			ForwardPass(ortho, voxelVS);

			ShaderControl->BindPipeline<VoxelRasterVS, VoxelRasterGS, VoxelConvertFS>(GlobalShaders);
			ForwardPass(ortho, voxelVS);
		}

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

		VoxelColors colors = {unpackedVoxels};
		return colors;
	}

	void Renderer::RenderPass(ecs::View &view, shared_ptr<RenderTarget> shadowMap, VoxelColors voxelGrid)
	{
		RenderPhase phase("RenderPass", timer);

		EngineRenderTargets targets;
		targets.GBuffer0 = RTPool->Get({ PF_RGBA8, view.extents });
		targets.GBuffer1 = RTPool->Get({ PF_RGBA16F, view.extents });
		targets.Depth = RTPool->Get({ PF_DEPTH32F, view.extents });
		targets.ShadowMap = shadowMap;
		targets.VoxelGrid = voxelGrid;

		Texture attachments[] =
		{
			targets.GBuffer0->GetTexture(),
			targets.GBuffer1->GetTexture(),
		};

		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		SetRenderTargets(2, attachments, &targets.Depth->GetTexture());

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
