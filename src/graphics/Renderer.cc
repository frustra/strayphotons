#include "graphics/Renderer.hh"
#include "graphics/RenderTargetPool.hh"
#include "graphics/Shader.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/postprocess/PostProcess.hh"
#include "core/Game.hh"
#include "core/Logging.hh"
#include "ecs/components/Renderable.hh"

namespace sp
{
	class SceneVS : public Shader
	{
		SHADER_TYPE(SceneVS)

		SceneVS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
			Bind(projection, "projMatrix");
			Bind(model, "modelMatrix");
			Bind(view, "viewMatrix");
		}

		void SetParameters(glm::mat4 newProj, glm::mat4 newView, glm::mat4 newModel)
		{
			Set(projection, newProj);
			Set(view, newView);
			Set(model, newModel);
		}

		void SetView(glm::mat4 newView)
		{
			Set(view, newView);
		}

		void SetModel(glm::mat4 newModel)
		{
			Set(model, newModel);
		}

	private:
		Uniform projection, model, view;
	};

	class SceneFS : public Shader
	{
		SHADER_TYPE(SceneFS)
		using Shader::Shader;
	};

	IMPLEMENT_SHADER_TYPE(SceneVS, "scene.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(SceneFS, "scene.frag", Fragment);

	Renderer::Renderer(Game *game) : GraphicsContext(game)
	{
	}

	Renderer::~Renderer()
	{
		if (ShaderManager)
			delete ShaderManager;
	}

	// TODO Clean up Renderable when unloaded.
	void PrepareRenderable(ECS::Renderable *comp)
	{
		for (auto primitive : comp->model->primitives)
		{
			primitive->indexBufferHandle = comp->model->LoadBuffer(primitive->indexBuffer.bufferName);
			if (primitive->textureName[0]) primitive->textureHandle = comp->model->LoadTexture(primitive->textureName);

			glCreateVertexArrays(1, &primitive->vertexBufferHandle);
			for (int i = 0; i < 3; i++)
			{
				auto *attr = &primitive->attributes[i];
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
			if (primitive->textureHandle) glBindTextures(0, 1, &primitive->textureHandle);
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

		ShaderManager = new sp::ShaderManager();
		ShaderManager->CompileAll(*ShaderSet);

		auto projection = glm::perspective(glm::radians(60.0f), 1.778f, 0.1f, 256.0f);
		auto view = glm::translate(glm::mat4(), glm::vec3(0.0f, -1.0f, -2.5f));
		auto model = glm::mat4();

		auto sceneVS = ShaderSet->Get<SceneVS>();
		sceneVS->SetParameters(projection, view, model);

		for (Entity ent : game->entityManager.EntitiesWith<ECS::Renderable>())
		{
			auto comp = ent.Get<ECS::Renderable>();
			PrepareRenderable(comp);
		}

		AssertGLOK("Renderer::Prepare");
	}

	void Renderer::RenderFrame()
	{
		RTPool->TickFrame();

		auto sceneVS = ShaderSet->Get<SceneVS>();

		static float rot = 0;
		auto rotMat = glm::rotate(glm::mat4(), rot, glm::vec3(0.f, 1.f, 0.f));
		sceneVS->SetModel(rotMat);
		rot += 0.01;

		EngineRenderTargets targets;
		targets.GBuffer0 = RTPool->Get(RenderTargetDesc(PF_RGBA8, { 1280, 720 }));
		targets.GBuffer1 = RTPool->Get(RenderTargetDesc(PF_RGBA8, { 1280, 720 }));
		targets.DepthStencil = RTPool->Get(RenderTargetDesc(PF_DEPTH_COMPONENT24, { 1280, 720 }));

		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);

		Texture attachments[] =
		{
			targets.GBuffer0->GetTexture(),
			targets.GBuffer1->GetTexture(),
		};

		SetRenderTargets(2, attachments, &targets.DepthStencil->GetTexture());

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		ShaderManager->BindPipeline<SceneVS, SceneFS>(*ShaderSet);

		for (Entity ent : game->entityManager.EntitiesWith<ECS::Renderable>())
		{
			auto comp = ent.Get<ECS::Renderable>();
			DrawRenderable(comp);
		}

		// Run postprocessing.
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		PostProcessing::Process(this, targets);

		AssertGLOK("Renderer::RenderFrame");
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
