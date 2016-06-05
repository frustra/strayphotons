#pragma once

#include "Graphics.hh"

namespace sp
{
	struct Attribute
	{
		GLuint index;
		GLuint elements;
		GLenum type;
		GLuint offset;
	};

	struct TextureVertex
	{
		glm::vec3 position;
		glm::vec2 uv;

		static vector<Attribute> Attributes()
		{
			return
			{
				{ 0, 3, GL_FLOAT, 0 },
				{ 2, 2, GL_FLOAT, sizeof(position) },
			};
		}
	};

	class VertexBuffer
	{
	public:
		VertexBuffer() { }

		VertexBuffer Create()
		{
			glCreateBuffers(1, &vbo);
			return *this;
		}

		VertexBuffer CreateVAO()
		{
			glCreateVertexArrays(1, &vao);
			return *this;
		}

		template <typename T>
		VertexBuffer SetElementsVAO(size_t n, T *buffer, GLenum usage = GL_STATIC_DRAW)
		{
			elements = n;

			if (vao == 0)
				Create();

			glNamedBufferData(vbo, n * sizeof(T), buffer, usage);

			if (vao == 0)
			{
				CreateVAO();

				for (auto attrib : T::Attributes())
				{
					glEnableVertexArrayAttrib(vao, attrib.index);
					glVertexArrayAttribFormat(vao, attrib.index, attrib.elements, attrib.type, false, attrib.offset);
					glVertexArrayVertexBuffer(vao, attrib.index, vbo, 0, sizeof(T));
				}
			}

			return *this;
		}

		void BindVAO() const
		{
			glBindVertexArray(vao);
		}

		bool Initialized() const
		{
			return vbo != 0;
		}

		size_t Elements() const
		{
			return elements;
		}

	private:
		GLuint vbo = 0, vao = 0, ibo = 0;
		size_t elements = 0;
	};
}