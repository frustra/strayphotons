#pragma once

#include "Graphics.hh"
#include "PixelFormat.hh"

namespace sp {
    struct GLBuffer {
        GLuint handle = 0;
        GLsizei size = 0;

        GLBuffer &Create();
        GLBuffer &Delete();

        void Bind(GLenum target) const;
        void Bind(GLenum target, GLuint index, GLintptr offset = 0, GLsizeiptr size = -1) const;
        GLBuffer &ClearRegion(GLPixelFormat format,
                              GLintptr offset = 0,
                              GLsizeiptr size = -1,
                              const void *data = nullptr);
        GLBuffer &ClearRegion(PixelFormat format,
                              GLintptr offset = 0,
                              GLsizeiptr size = -1,
                              const void *data = nullptr);
        GLBuffer &Clear(GLPixelFormat format, const void *data = nullptr);
        GLBuffer &Clear(PixelFormat format, const void *data = nullptr);

        GLBuffer &Data(GLsizei size, const void *data = nullptr, GLenum usage = GL_STATIC_DRAW);
        void *Map(GLenum access);
        GLBuffer &Unmap();

        bool operator==(const GLBuffer &other) const {
            return other.handle == handle;
        }

        bool operator!=(const GLBuffer &other) const {
            return !(*this == other);
        }

        bool operator!() const {
            return !handle;
        }

        operator bool() const {
            return !!handle;
        }
    };
} // namespace sp
