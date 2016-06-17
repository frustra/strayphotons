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

		void SetParams(const ECS::View &view, glm::mat4 modelMat)
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

	static CVar<float> FlashlightIntensity("r.Flashlight", 2000, "Flashlight intensity");

	// TODO Clean up Renderable when unloaded.
	void PrepareRenderable(ECS::Renderable *comp)
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

	void DrawRenderable(ECS::Renderable *comp)
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

		for (Entity ent : game->entityManager.EntitiesWith<ECS::Renderable>())
		{
			auto comp = ent.Get<ECS::Renderable>();
			PrepareRenderable(comp);
		}

		AssertGLOK("Renderer::Prepare");
	}

	ECS::View *updateLightCaches(Entity entity, ECS::Light *light)
	{
		auto *view = entity.Get<ECS::View>();

		view->aspect = (float)view->extents.x / (float)view->extents.y;
		view->projMat = glm::perspective(light->spotAngle * 2.0f, view->aspect, view->clip[0], view->clip[1]);
		view->invProjMat = glm::inverse(view->projMat);

		auto *transform = entity.Get<ECS::Transform>();
		view->invViewMat = transform->GetModelTransform(*entity.GetManager());
		view->viewMat = glm::inverse(view->invViewMat);

		return view;
	}

	shared_ptr<RenderTarget> Renderer::RenderShadowMaps()
	{
		// TODO(xthexder): Handle lights without shadowmaps
		glm::ivec2 renderTargetSize;
		bool first = true;
		for (auto entity : game->entityManager.EntitiesWith<ECS::Light>())
		{
			auto light = entity.Get<ECS::Light>();
			if (first)
			{
				light->intensity = FlashlightIntensity.Get();
				first = false;
			}
			if (entity.Has<ECS::View>())
			{
				auto view = updateLightCaches(entity, light);
				light->mapOffset = glm::vec4(renderTargetSize.x, 0, view->extents.x, view->extents.y);
				view->offset = glm::ivec2(light->mapOffset);

				renderTargetSize.x += view->extents.x;
				if (view->extents.y > renderTargetSize.y)
					renderTargetSize.y = view->extents.y;
			}
		}

		auto renderTarget = RTPool->Get(RenderTargetDesc(PF_DEPTH32F, renderTargetSize));
		SetRenderTargets(0, nullptr, &renderTarget->GetTexture());

		for (auto entity : game->entityManager.EntitiesWith<ECS::Light>())
		{
			auto light = entity.Get<ECS::Light>();
			if (entity.Has<ECS::View>())
			{
				light->mapOffset /= glm::vec4(renderTargetSize, renderTargetSize);

				auto view = entity.Get<ECS::View>();
				ForwardPass(*view);
			}
		}

		return renderTarget;
	}

	void Renderer::RenderPass(ECS::View &view, shared_ptr<RenderTarget> shadowMap)
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

		ECS::View forwardPassView = view;
		forwardPassView.offset = glm::ivec2();
		ForwardPass(forwardPassView);

		// Run postprocessing.
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		PostProcessing::Process(this, game, view, targets);

		//AssertGLOK("Renderer::RenderFrame");
	}

	void Renderer::ForwardPass(ECS::View &view)
	{
		RenderPhase phase("ForwardPass", timer);

		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_SCISSOR_TEST);
		glDisable(GL_BLEND);
		glDisable(GL_STENCIL_TEST);
		glDepthMask(GL_TRUE);

		glViewport(view.offset.x, view.offset.y, view.extents.x, view.extents.y);
		glScissor(view.offset.x, view.offset.y, view.extents.x, view.extents.y);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		ShaderControl->BindPipeline<SceneVS, SceneFS>(GlobalShaders);

		auto sceneVS = GlobalShaders->Get<SceneVS>();

		for (Entity ent : game->entityManager.EntitiesWith<ECS::Renderable, ECS::Transform>())
		{
			auto comp = ent.Get<ECS::Renderable>();
			auto modelMat = ent.Get<ECS::Transform>()->GetModelTransform(*ent.GetManager());
			sceneVS->SetParams(view, modelMat);
			DrawRenderable(comp);
		}
	}

	void Renderer::EndFrame()
	{
		// TODO(pushrax) remove
		// Begin compute example.
		/*auto exampleRT = RTPool->Get(RenderTargetDesc(PF_RGBA8, { 256, 256 }));
		exampleRT->GetTexture().BindImage(0, GL_WRITE_ONLY);
		ShaderControl->BindPipeline<ExampleCS>(GlobalShaders);
		glDispatchCompute(256 / 16, 256 / 16, 1);

		// Draw resulting texture from compute example.
		exampleRT->GetTexture().Bind(0);
		ShaderControl->BindPipeline<BasicPostVS, ScreenCoverFS>(GlobalShaders);
		glViewport(0, 0, 256, 256);
		DrawScreenCover();*/
		// End compute example.

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
