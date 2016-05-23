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

	void Renderer::Prepare()
	{
		shaderManager = new ShaderManager();
		shaderManager->CompileAll(*shaderSet);

		auto projection = glm::perspective(glm::radians(60.0f), 1.778f, 0.1f, 256.0f);
		auto view = glm::translate(glm::mat4(), glm::vec3(0.0f, -1.0f, -2.5f));
		auto model = glm::mat4();

		auto triangleVS = shaderSet->Get<TriangleVS>();
		triangleVS->SetParameters(projection, view, model);

		struct Vertex
		{
			float pos[3], color[3];
		};

		// Vertex vertexBuf[] =
		// {
		// 	{ { 1, 1, 0 }, { 1, 0, 0 }},
		// 	{ { -1, 1, 0 }, { 0, 1, 0 }},
		// 	{ { 0, -1, 0 }, { 0, 0, 1 }},
		// };

		std::map<string, GLuint> bufs;
		std::map<string, GLuint> texs;
		for (auto buffer : game->duck->scene.buffers)
		{
			GLuint handle;
			glCreateBuffers(1, &handle);
			glNamedBufferData(handle, buffer.second.data.size(), buffer.second.data.data(), GL_STATIC_DRAW);
			bufs[buffer.first] = handle;
		}

		for (auto texture : game->duck->scene.textures)
		{
			GLuint handle;
			glCreateTextures(GL_TEXTURE_2D, 1, &handle);
			auto img = game->duck->scene.images[texture.second.source];
			glTextureStorage2D(handle, 1, GL_RGBA8, img.width, img.height);
			glTextureSubImage2D(handle, 0, 0, 0, img.width, img.height, GL_RGB, texture.second.type, img.image.data());
			texs[texture.first] = handle;
			texHandle = handle;
		}

		for (auto node : game->duck->list_nodes())
		{
			for (auto mesh : node.node->meshes)
			{
				for (auto primitive : game->duck->scene.meshes[mesh].primitives)
				{
					tinygltf::Accessor iAcc = game->duck->scene.accessors[primitive.indices];
					tinygltf::Accessor pAcc = game->duck->scene.accessors[primitive.attributes["POSITION"]];
					tinygltf::Accessor nAcc = game->duck->scene.accessors[primitive.attributes["NORMAL"]];
					tinygltf::Accessor tAcc = game->duck->scene.accessors[primitive.attributes["TEXCOORD_0"]];
					tinygltf::BufferView iBufView = game->duck->scene.bufferViews[iAcc.bufferView];
					tinygltf::BufferView pBufView = game->duck->scene.bufferViews[pAcc.bufferView];
					tinygltf::BufferView nBufView = game->duck->scene.bufferViews[nAcc.bufferView];
					tinygltf::BufferView tBufView = game->duck->scene.bufferViews[tAcc.bufferView];

					indexBuffer = bufs[iBufView.buffer];
					numElems = iAcc.count;

					glCreateVertexArrays(1, &vertexAttribs);
					glEnableVertexArrayAttrib(vertexAttribs, 0);
					glVertexArrayAttribFormat(vertexAttribs, 0, 3, pAcc.componentType, GL_FALSE, 0);
					glVertexArrayVertexBuffer(vertexAttribs, 0, bufs[pBufView.buffer], pAcc.byteOffset + pBufView.byteOffset, pAcc.byteStride);
					glEnableVertexArrayAttrib(vertexAttribs, 1);
					glVertexArrayAttribFormat(vertexAttribs, 1, 3, nAcc.componentType, GL_FALSE, 0);
					glVertexArrayVertexBuffer(vertexAttribs, 1, bufs[nBufView.buffer], nAcc.byteOffset + nBufView.byteOffset, nAcc.byteStride);
					glEnableVertexArrayAttrib(vertexAttribs, 2);
					glVertexArrayAttribFormat(vertexAttribs, 2, 2, tAcc.componentType, GL_FALSE, 0);
					glVertexArrayVertexBuffer(vertexAttribs, 2, bufs[tBufView.buffer], tAcc.byteOffset + tBufView.byteOffset, tAcc.byteStride);
				}
			}
		}

		// Create vertex buffer
		// glCreateBuffers(1, &vertices);
		// glNamedBufferData(vertices, sizeof(vertexBuf), vertexBuf, GL_STATIC_DRAW);

		// glCreateBuffers(1, &indexBuffer);
		// glNamedBufferData(indexBuffer, sizeof(indexBuf), indexBuf, GL_STATIC_DRAW);

		// glCreateVertexArrays(1, &vertexAttribs);
		// glEnableVertexArrayAttrib(vertexAttribs, 0);
		// glVertexArrayAttribFormat(vertexAttribs, 0, 3, GL_FLOAT, GL_FALSE, 0);
		// glVertexArrayVertexBuffer(vertexAttribs, 0, vertices, 0, sizeof(Vertex));
		// glEnableVertexArrayAttrib(vertexAttribs, 1);
		// glVertexArrayAttribFormat(vertexAttribs, 1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
		// glVertexArrayVertexBuffer(vertexAttribs, 1, vertices, 0, sizeof(Vertex));

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

		glBindVertexArray(vertexAttribs);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
		glBindTextures(0, 1, &texHandle);
		glDrawElements(GL_TRIANGLES, numElems, GL_UNSIGNED_SHORT, nullptr);

		glfwSwapBuffers(window);
		AssertGLOK("Renderer::RenderFrame");
	}
}
