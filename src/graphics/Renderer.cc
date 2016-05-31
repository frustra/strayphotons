#include "graphics/Renderer.hh"
#include "graphics/Shader.hh"
#include "core/Logging.hh"
#include "ecs/components/Renderable.hh"

namespace sp
{
	class TriangleVS : public Shader
	{
		SHADER_TYPE(TriangleVS)

		TriangleVS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
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

	class TriangleFS : public Shader
	{
		SHADER_TYPE(TriangleFS)
		using Shader::Shader;
	};

	IMPLEMENT_SHADER_TYPE(TriangleVS, "triangle.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(TriangleFS, "triangle.frag", Fragment);

	Renderer::~Renderer()
	{
		if (shaderManager)
			delete shaderManager;
	}

	void PrepareRenderable(ECS::Renderable *comp)
	{
		std::map<string, GLuint> bufs;
		std::map<string, GLuint> texs;
		for (auto buffer : comp->model->scene->buffers)
		{
			GLuint handle;
			glCreateBuffers(1, &handle);
			glNamedBufferData(handle, buffer.second.data.size(), buffer.second.data.data(), GL_STATIC_DRAW);
			bufs[buffer.first] = handle;
		}

		for (auto texture : comp->model->scene->textures)
		{
			GLuint handle;
			glCreateTextures(GL_TEXTURE_2D, 1, &handle);
			auto img = comp->model->scene->images[texture.second.source];
			glTextureStorage2D(handle, 1, GL_RGBA8, img.width, img.height);
			glTextureSubImage2D(handle, 0, 0, 0, img.width, img.height, GL_RGB, texture.second.type, img.image.data());
			texs[texture.first] = handle;
		}

		for (auto primitive : comp->model->primitives)
		{
			primitive->indexBufferHandle = bufs[primitive->indexBuffer.bufferName];
			primitive->textureHandle = texs[primitive->textureName];

			glCreateVertexArrays(1, &primitive->vertexBufferHandle);
			for (int i = 0; i < 3; i++)
			{
				auto *attr = &primitive->attributes[i];
				glEnableVertexArrayAttrib(primitive->vertexBufferHandle, i);
				glVertexArrayAttribFormat(primitive->vertexBufferHandle, i, attr->componentCount, attr->componentType, GL_FALSE, 0);
				glVertexArrayVertexBuffer(primitive->vertexBufferHandle, i, bufs[attr->bufferName], attr->byteOffset, attr->byteStride);
			}
		}
	}

	void Renderer::Prepare()
	{
		shaderManager = new ShaderManager();
		shaderManager->CompileAll(*shaderSet);

		auto projection = glm::perspective(glm::radians(60.0f), 1.778f, 0.1f, 256.0f);
		auto view = glm::translate(glm::mat4(), glm::vec3(0.0f, -1.0f, -2.5f));
		auto model = glm::mat4();

		auto triangleVS = shaderSet->Get<TriangleVS>();
		triangleVS->SetParameters(projection, view, model);

		for (Entity ent : game->entityManager.EntitiesWith<ECS::Renderable>())
		{
			auto comp = ent.Get<ECS::Renderable>();
			PrepareRenderable(comp);
		}

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

		AssertGLOK("Renderer::Prepare");
	}

	void Renderer::RenderFrame()
	{
		auto triangleVS = shaderSet->Get<TriangleVS>();

		static float rot = 0;
		auto rotMat = glm::rotate(glm::mat4(), rot, glm::vec3(0.f, 1.f, 0.f));
		triangleVS->SetModel(rotMat);
		rot += 0.01;

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		shaderManager->BindPipeline<TriangleVS, TriangleFS>(*shaderSet);

		for (Entity ent : game->entityManager.EntitiesWith<ECS::Renderable>())
		{
			auto comp = ent.Get<ECS::Renderable>();

			for (auto primitive : comp->model->primitives)
			{
				glBindVertexArray(primitive->vertexBufferHandle);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, primitive->indexBufferHandle);
				glBindTextures(0, 1, &primitive->textureHandle);
				glDrawElements(
					primitive->drawMode,
					primitive->indexBuffer.componentCount,
					primitive->indexBuffer.componentType,
					(char *) primitive->indexBuffer.byteOffset
				);
			}
		}

		glfwSwapBuffers(window);
		AssertGLOK("Renderer::RenderFrame");
	}
}
