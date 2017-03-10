#include "graphics/basic_renderer/BasicRenderer.hh"
#include "core/Game.hh"
#include "core/Logging.hh"
#include "core/CVar.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/View.hh"

namespace sp
{
	BasicRenderer::BasicRenderer(Game *game) : GraphicsContext(game)
	{
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	}

	BasicRenderer::~BasicRenderer()
	{
	}

	void BasicRenderer::PrepareRenderable(ecs::Handle<ecs::Renderable> comp)
	{
		for (auto primitive : comp->model->primitives)
		{
			auto indexBuffer = comp->model->GetBuffer(primitive->indexBuffer.bufferName);

			GLModel::Primitive glPrimitive;

			glGenBuffers(1, &glPrimitive.indexBufferHandle);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glPrimitive.indexBufferHandle);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBuffer.size(), indexBuffer.data(), GL_STATIC_DRAW);

			glGenVertexArrays(1, &glPrimitive.vertexBufferHandle);
			glBindVertexArray(glPrimitive.vertexBufferHandle);
			for (int i = 0; i < 3; i++)
			{
				auto *attr = &primitive->attributes[i];
				if (attr->componentCount == 0) continue;

				auto attribBuffer = comp->model->GetBuffer(attr->bufferName);
				GLuint attribBufferHandle;
				glGenBuffers(1, &attribBufferHandle);
				glBindBuffer(GL_ARRAY_BUFFER, attribBufferHandle);
				glBufferData(GL_ARRAY_BUFFER, attribBuffer.size(), attribBuffer.data(), GL_STATIC_DRAW);

				glVertexAttribPointer(i, attr->componentCount, attr->componentType, GL_FALSE, attr->byteStride, (void *) attr->byteOffset);
				glEnableVertexAttribArray(i);
			}

			primitiveMap[primitive] = glPrimitive;
		}
	}

	void BasicRenderer::DrawRenderable(ecs::Handle<ecs::Renderable> comp)
	{
		for (auto primitive : comp->model->primitives)
		{
			if (!primitiveMap.count(primitive))
			{
				PrepareRenderable(comp);
			}

			auto glPrimitive = primitiveMap[primitive];
			glBindVertexArray(glPrimitive.vertexBufferHandle);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glPrimitive.indexBufferHandle);

			glDrawElements(
				primitive->drawMode,
				primitive->indexBuffer.components,
				primitive->indexBuffer.componentType,
				(char *) primitive->indexBuffer.byteOffset
			);
		}
	}

	GLuint compileShader(GLenum type, const char *src)
	{
		auto shader = glCreateShader(type);
		int success;

		glShaderSource(shader, 1, &src, nullptr);
		glCompileShader(shader);
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

		if (!success)
		{
			int llen = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &llen);

			vector<char> log(llen);
			glGetShaderInfoLog(shader, llen, &llen, &log[0]);
			Errorf("%s", string(&log[0]));

			Assert(false, "compiling shader");
		}

		return shader;
	}

	void BasicRenderer::Prepare()
	{
		glEnable(GL_FRAMEBUFFER_SRGB);

		const char *vtxShaderSrc = R"(
			#version 410

			layout (location = 0) in vec3 inPos;
			layout (location = 1) in vec3 inNormal;
			layout (location = 2) in vec2 inTexCoord;

			uniform mat4 mvpMatrix;

			out vec3 vNormal;
			out vec2 vTexCoord;

			void main()
			{
				gl_Position = mvpMatrix * vec4(inPos, 1.0);
				vNormal = inNormal;
				vTexCoord = inTexCoord;
			}
		)";

		const char *fragShaderSrc = R"(
			#version 410

			in vec3 vNormal;
			in vec2 vTexCoord;

			layout (location = 0) out vec4 frameBuffer;

			void main()
			{
				frameBuffer.rgb = (vNormal * 0.5) + vec3(0.5);
			}
		)";

		sceneProgram = glCreateProgram();

		auto vtxShader = compileShader(GL_VERTEX_SHADER, vtxShaderSrc);
		auto fragShader = compileShader(GL_FRAGMENT_SHADER, fragShaderSrc);
		glAttachShader(sceneProgram, vtxShader);
		glAttachShader(sceneProgram, fragShader);

		GLint success;
		glLinkProgram(sceneProgram);
		glGetProgramiv(sceneProgram, GL_LINK_STATUS, &success);

		if (!success)
		{
			int llen = 0;
			glGetProgramiv(sceneProgram, GL_INFO_LOG_LENGTH, &llen);

			vector<char> log(llen);
			glGetProgramInfoLog(sceneProgram, llen, &llen, &log[0]);

			Errorf("%s", string(&log[0]));
			Assert(false, "linking shader program");
		}

		glDetachShader(sceneProgram, vtxShader);
		glDetachShader(sceneProgram, fragShader);
		glDeleteShader(vtxShader);
		glDeleteShader(fragShader);

		AssertGLOK("BasicRenderer::Prepare");
	}

	void BasicRenderer::RenderPass(ecs::View &viewRef)
	{
		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);

		ecs::View view = viewRef;
		view.offset = glm::ivec2();
		PrepareForView(view);

		glUseProgram(sceneProgram);

		auto mvpLoc = glGetUniformLocation(sceneProgram, "mvpMatrix");

		for (ecs::Entity ent : game->entityManager.EntitiesWith<ecs::Renderable, ecs::Transform>())
		{
			auto comp = ent.Get<ecs::Renderable>();
			auto modelMat = ent.Get<ecs::Transform>()->GetGlobalTransform();
			auto mvp = view.projMat * view.viewMat * modelMat;

			glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
			DrawRenderable(comp);
		}

		//AssertGLOK("BasicRenderer::RenderFrame");
	}

	void BasicRenderer::PrepareForView(ecs::View &view)
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

	void BasicRenderer::RenderLoading(ecs::View &view)
	{
		// TODO(xthexder): Put something here
	}

	void BasicRenderer::BeginFrame()
	{
	}

	void BasicRenderer::EndFrame()
	{
		glfwSwapBuffers(window);
	}
}
