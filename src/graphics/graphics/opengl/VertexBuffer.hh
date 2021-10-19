#pragma once

#include "Graphics.hh"
#include "core/Common.hh"

#include <vector>

namespace sp {
    struct Attribute {
        GLuint index;
        GLuint elements;
        GLenum type;
        GLuint offset;
    };

    struct TextureVertex {
        glm::vec3 position;
        glm::vec2 uv;

        static std::vector<Attribute> Attributes() {
            return {
                {0, 3, GL_FLOAT, 0},
                {2, 2, GL_FLOAT, sizeof(position)},
            };
        }
    };

    struct SceneVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;

        static std::vector<Attribute> Attributes() {
            return {
                {0, 3, GL_FLOAT, 0},
                {1, 3, GL_FLOAT, sizeof(position)},
                {2, 2, GL_FLOAT, sizeof(position) + sizeof(normal)},
            };
        }
    };

    class VertexBuffer {
    public:
        VertexBuffer() {}

        VertexBuffer &Create() {
            Assert(vbo == 0, "vertex buffer already created");
            glCreateBuffers(1, &vbo);
            return *this;
        }

        VertexBuffer &CreateVAO() {
            Assert(vao == 0, "vertex array already created");
            glCreateVertexArrays(1, &vao);
            return *this;
        }

        VertexBuffer &Destroy() {
            Assert(vbo != 0, "vertex buffer not created");
            glDeleteBuffers(1, &vbo);
            vbo = 0;
            return *this;
        }

        VertexBuffer &DestroyVAO() {
            Assert(vao != 0, "vertex array not created");
            glDeleteVertexArrays(1, &vao);
            vao = 0;
            return *this;
        }

        template<typename T>
        VertexBuffer &SetElements(GLsizei n, T *buffer, GLenum usage = GL_STATIC_DRAW) {
            elements = n;
            glNamedBufferData(vbo, n * sizeof(T), buffer, usage);
            return *this;
        }

        template<typename T>
        VertexBuffer &SetElementsVAO(GLsizei n, T *buffer, GLenum usage = GL_STATIC_DRAW) {
            elements = n;

            if (vbo == 0) Create();

            glNamedBufferData(vbo, n * sizeof(T), buffer, usage);

            if (vao == 0) {
                CreateVAO();

                for (auto attrib : T::Attributes()) {
                    EnableAttrib(attrib.index, attrib.elements, attrib.type, false, attrib.offset, sizeof(T));
                }
            }

            return *this;
        }

        VertexBuffer &EnableAttrib(GLuint index,
            GLint size,
            GLenum type,
            bool normalized = false,
            GLuint offset = 0,
            GLsizei stride = 0) {
            Assert(vao != 0, "vertex array not created");
            glEnableVertexArrayAttrib(vao, index);
            glVertexArrayAttribFormat(vao, index, size, type, normalized, offset);

            if (stride > 0) { SetAttribBuffer(index, stride); }
            return *this;
        }

        VertexBuffer &SetAttribBuffer(GLuint index, GLsizei stride, GLintptr offset = 0) {
            Assert(vao != 0, "vertex array not created");
            glVertexArrayVertexBuffer(vao, index, vbo, offset, stride);
            return *this;
        }

        void BindVAO() const {
            Assert(vao != 0, "vertex array not created");
            glBindVertexArray(vao);
        }

        void BindElementArray() const {
            Assert(vbo != 0, "vertex buffer not created");
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);
        }

        bool Initialized() const {
            return vbo != 0;
        }

        GLsizei Elements() const {
            return elements;
        }

        GLuint VAO() const {
            return vao;
        }

        GLuint VBO() const {
            return vbo;
        }

    private:
        GLuint vbo = 0, vao = 0;
        GLsizei elements = 0;
    };
} // namespace sp
