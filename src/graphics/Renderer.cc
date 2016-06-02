#include "graphics/Renderer.hh"
#include "graphics/Shader.hh"
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

	class BasicPostVS : public Shader
	{
		SHADER_TYPE(BasicPostVS)
		using Shader::Shader;
	};

	class ScreenCoverFS : public Shader
	{
		SHADER_TYPE(ScreenCoverFS)
		using Shader::Shader;
	};

	IMPLEMENT_SHADER_TYPE(BasicPostVS, "basic_post.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(ScreenCoverFS, "screen_cover.frag", Fragment);

	const float screenCoverElements[][5] =
	{
		{ -2, -1, 0, -0.5, 0},
		{2, -1, 0, 1.5, 0},
		{0, 3, 0, 0.5, 2},
	};

	Renderer::~Renderer()
	{
		if (shaderManager)
			delete shaderManager;
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
		shaderManager = new ShaderManager();
		shaderManager->CompileAll(*shaderSet);

		auto projection = glm::perspective(glm::radians(60.0f), 1.778f, 0.1f, 256.0f);
		auto view = glm::translate(glm::mat4(), glm::vec3(0.0f, -1.0f, -2.5f));
		auto model = glm::mat4();

		auto sceneVS = shaderSet->Get<SceneVS>();
		sceneVS->SetParameters(projection, view, model);

		for (Entity ent : game->entityManager.EntitiesWith<ECS::Renderable>())
		{
			auto comp = ent.Get<ECS::Renderable>();
			PrepareRenderable(comp);
		}

		// TODO(pushrax) remove
		glCreateBuffers(1, &screenCoverVBO);
		glNamedBufferData(screenCoverVBO, sizeof(screenCoverElements), screenCoverElements, GL_STATIC_DRAW);
		glCreateVertexArrays(1, &screenCoverVAO);
		glEnableVertexArrayAttrib(screenCoverVAO, 0);
		glVertexArrayAttribFormat(screenCoverVAO, 0, 3, GL_FLOAT, false, 0);
		glVertexArrayVertexBuffer(screenCoverVAO, 0, screenCoverVBO, 0, 5 * sizeof(float));
		glEnableVertexArrayAttrib(screenCoverVAO, 2);
		glVertexArrayAttribFormat(screenCoverVAO, 2, 2, GL_FLOAT, false, 3 * sizeof(float));
		glVertexArrayVertexBuffer(screenCoverVAO, 2, screenCoverVBO, 0, 5 * sizeof(float));

		fbcolor.Create().Filter(GL_NEAREST, GL_NEAREST).Size(1280, 720).Storage2D(PF_RGBA8);
		fbdepth.Create().Filter(GL_NEAREST, GL_NEAREST).Size(1280, 720).Storage2D(PF_DEPTH_COMPONENT16);

		glCreateFramebuffers(1, &fb);
		glNamedFramebufferTexture(fb, GL_COLOR_ATTACHMENT0, fbcolor.handle, 0);
		glNamedFramebufferTexture(fb, GL_DEPTH_ATTACHMENT, fbdepth.handle, 0);

		AssertGLOK("Renderer::Prepare");
	}

	void Renderer::RenderFrame()
	{
		auto sceneVS = shaderSet->Get<SceneVS>();

		static float rot = 0;
		auto rotMat = glm::rotate(glm::mat4(), rot, glm::vec3(0.f, 1.f, 0.f));
		sceneVS->SetModel(rotMat);
		rot += 0.01;

		glBindFramebuffer(GL_FRAMEBUFFER, fb);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		shaderManager->BindPipeline<SceneVS, SceneFS>(*shaderSet);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

		for (Entity ent : game->entityManager.EntitiesWith<ECS::Renderable>())
		{
			auto comp = ent.Get<ECS::Renderable>();
			DrawRenderable(comp);
		}

		// Draw framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		shaderManager->BindPipeline<BasicPostVS, ScreenCoverFS>(*shaderSet);

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);

		fbcolor.Bind(0);
		fbdepth.Bind(1);
		glBindVertexArray(screenCoverVAO);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glfwSwapBuffers(window);
		AssertGLOK("Renderer::RenderFrame");
	}
}
