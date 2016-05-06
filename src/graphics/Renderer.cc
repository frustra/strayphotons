#include "graphics/Renderer.hh"
#include "graphics/Shader.hh"
#include "core/Logging.hh"

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

		void SetParameters(glm::mat4 newProj, glm::mat4 newModel, glm::mat4 newView)
		{
			Set(projection, newProj);
			Set(model, newModel);
			Set(view, newView);
		}

		void SetView(glm::mat4 newView)
		{
			Set(view, newView);
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

	void Renderer::Prepare()
	{
		shaderManager = new ShaderManager();
		shaderManager->CompileAll(*shaderSet);

		auto projection = glm::perspective(glm::radians(60.0f), 1.778f, 0.1f, 256.0f);
		auto view = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, -2.5f));
		auto model = glm::mat4();

		auto triangleVS = shaderSet->Get<TriangleVS>();
		triangleVS->SetParameters(projection, view, model);

		struct Vertex
		{
			float pos[3], color[3];
		};

		Vertex vertexBuf[] =
		{
			{ { 1, 1, 0 }, { 1, 0, 0 }},
			{ { -1, 1, 0 }, { 0, 1, 0 }},
			{ { 0, -1, 0 }, { 0, 0, 1 }},
		};

		uint32 indexBuf[] = { 0, 1, 2 };

		// Create vertex buffer
		glCreateBuffers(1, &vertices);
		glNamedBufferData(vertices, sizeof(vertexBuf), vertexBuf, GL_STATIC_DRAW);

		glCreateVertexArrays(1, &vertexAttribs);
		glEnableVertexArrayAttrib(vertexAttribs, 0);
		glVertexArrayAttribFormat(vertexAttribs, 0, 3, GL_FLOAT, GL_FALSE, 0);
		glVertexArrayVertexBuffer(vertexAttribs, 0, vertices, 0, sizeof(Vertex));
		glEnableVertexArrayAttrib(vertexAttribs, 1);
		glVertexArrayAttribFormat(vertexAttribs, 1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
		glVertexArrayVertexBuffer(vertexAttribs, 1, vertices, 0, sizeof(Vertex));

		AssertGLOK("Renderer::Prepare");
	}

	void Renderer::RenderFrame()
	{
		auto triangleVS = shaderSet->Get<TriangleVS>();

		static float rot = 0;
		auto rotMat = glm::rotate(glm::mat4(), rot, glm::vec3(0.f, 0.f, 1.f));
		auto trMat = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, -2.5f));
		triangleVS->SetView(rotMat * trMat);
		rot += 0.01;

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		shaderManager->BindPipeline<TriangleVS, TriangleFS>(*shaderSet);

		glBindBuffer(GL_ARRAY_BUFFER, vertices);
		glBindVertexArray(vertexAttribs);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glfwSwapBuffers(window);
		AssertGLOK("Renderer::RenderFrame");
	}
}
