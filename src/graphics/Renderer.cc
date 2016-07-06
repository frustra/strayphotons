#include "graphics/Renderer.hh"
#include "graphics/RenderTargetPool.hh"
#include "graphics/Shader.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/Util.hh"
#include "graphics/GenericShaders.hh"
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
	class SceneVS : public Shader
	{
		SHADER_TYPE(SceneVS)

		SceneVS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
			Bind(mvpMat, "mvpMat");
			Bind(normalMat, "normalMat");
		}

		void SetParams(const ecs::View &view, glm::mat4 modelMat)
		{
			Set(mvpMat, view.projMat * view.viewMat * modelMat);
			Set(normalMat, glm::mat3(view.viewMat * modelMat));
		}

	private:
		Uniform mvpMat, normalMat;
	};

	class SceneFS : public Shader
	{
		SHADER_TYPE(SceneFS)
		using Shader::Shader;
	};

	IMPLEMENT_SHADER_TYPE(SceneVS, "scene.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(SceneFS, "scene.frag", Fragment);

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
		Texture baseColorTex, roughnessTex;

		DefaultMaterial()
		{
			unsigned char baseColor[4] = { 255, 255, 255, 255 };
			unsigned char roughness[4] = { 200, 200, 200, 255 };

			baseColorTex.Create()
			.Filter(GL_NEAREST, GL_NEAREST).Wrap(GL_REPEAT, GL_REPEAT)
			.Size(1, 1).Storage2D(PF_RGB8).Image2D(baseColor);

			roughnessTex.Create()
			.Filter(GL_NEAREST, GL_NEAREST).Wrap(GL_REPEAT, GL_REPEAT)
			.Size(1, 1).Storage2D(PF_RGB8).Image2D(roughness);
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

			glDrawElements(
				primitive->drawMode,
				primitive->indexBuffer.componentCount,
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

		auto renderTarget = RTPool->Get(RenderTargetDesc(PF_DEPTH32F, renderTargetSize));
		SetRenderTargets(0, nullptr, &renderTarget->GetTexture());

		glViewport(0, 0, renderTargetSize.x, renderTargetSize.y);
		glDisable(GL_SCISSOR_TEST);
		glDepthMask(GL_TRUE);
		glClear(GL_DEPTH_BUFFER_BIT);

		for (auto entity : game->entityManager.EntitiesWith<ecs::Light>())
		{
			auto light = entity.Get<ecs::Light>();
			if (entity.Has<ecs::View>())
			{
				light->mapOffset /= glm::vec4(renderTargetSize, renderTargetSize);

				ecs::Handle<ecs::View> view = entity.Get<ecs::View>();
				ForwardPass(*view);
			}
		}

		return renderTarget;
	}

	void Renderer::RenderPass(ecs::View &view, shared_ptr<RenderTarget> shadowMap)
	{
		RenderPhase phase("RenderPass", timer);

		EngineRenderTargets targets;
		targets.GBuffer0 = RTPool->Get(RenderTargetDesc(PF_RGBA8, view.extents));
		targets.GBuffer1 = RTPool->Get(RenderTargetDesc(PF_RGBA16F, view.extents));
		targets.Depth = RTPool->Get(RenderTargetDesc(PF_DEPTH32F, view.extents));
		targets.ShadowMap = shadowMap;

		Texture attachments[] =
		{
			targets.GBuffer0->GetTexture(),
			targets.GBuffer1->GetTexture(),
		};

		SetRenderTargets(2, attachments, &targets.Depth->GetTexture());

		ecs::View forwardPassView = view;
		forwardPassView.offset = glm::ivec2();
		ForwardPass(forwardPassView);

		// Run postprocessing.
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		PostProcessing::Process(this, game, view, targets);

		//AssertGLOK("Renderer::RenderFrame");
	}

	void Renderer::ForwardPass(ecs::View &view)
	{
		RenderPhase phase("ForwardPass", timer);

		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
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

		if (CVarRenderWireframe.Get())
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		ShaderControl->BindPipeline<SceneVS, SceneFS>(GlobalShaders);

		auto sceneVS = GlobalShaders->Get<SceneVS>();

		for (ecs::Entity ent : game->entityManager.EntitiesWith<ecs::Renderable, ecs::Transform>())
		{
			auto comp = ent.Get<ecs::Renderable>();
			auto modelMat = ent.Get<ecs::Transform>()->GetModelTransform(*ent.GetManager());
			sceneVS->SetParams(view, modelMat);
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
