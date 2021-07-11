#include "GLBuffer.hh"

#include "core/Common.hh"

namespace sp {
    GLBuffer &GLBuffer::Create() {
        Assert(!handle, "buffer cannot be recreated");
        glCreateBuffers(1, &handle);
        return *this;
    }

    GLBuffer &GLBuffer::Delete() {
        if (handle) glDeleteBuffers(1, &handle);
        handle = 0;
        size = 0;
        return *this;
    }

    void GLBuffer::Bind(GLenum target) const {
        Assert(handle, "null buffer handle");
        glBindBuffer(target, handle);
    }

    void GLBuffer::Bind(GLenum target, GLuint index, GLintptr offset, GLsizeiptr size) const {
        Assert(handle, "null buffer handle");
        if (size < 0) size = this->size - offset;
        Assert(size >= 0, "binding offset is greater than size");
        glBindBufferRange(target, index, handle, offset, size);
    }

    GLBuffer &GLBuffer::ClearRegion(GLPixelFormat format, GLintptr offset, GLsizeiptr size, const void *data) {
        Assert(handle, "null buffer handle");
        if (size < 0) size = this->size - offset;
        Assert(size >= 0, "region offset is greater than size");
        // Bug in Nvidia Optimus driver, glClearNamedBufferSubData does not operate as expected.
        glBindBuffer(GL_COPY_READ_BUFFER, handle);
        glClearBufferSubData(GL_COPY_READ_BUFFER,
                             format.internalFormat,
                             offset,
                             size,
                             format.format,
                             format.type,
                             data);
        // glClearNamedBufferSubData(handle, format.internalFormat, offset, size, format.format, format.type, data);
        return *this;
    }

    GLBuffer &GLBuffer::ClearRegion(PixelFormat format, GLintptr offset, GLsizeiptr size, const void *data) {
        return ClearRegion(GLPixelFormat::PixelFormatMapping(format), offset, size, data);
    }

    GLBuffer &GLBuffer::Clear(GLPixelFormat format, const void *data) {
        Assert(handle, "null buffer handle");
        glClearNamedBufferData(handle, format.internalFormat, format.format, format.type, data);
        return *this;
    }

    GLBuffer &GLBuffer::Clear(PixelFormat format, const void *data) {
        return Clear(GLPixelFormat::PixelFormatMapping(format), data);
    }

    GLBuffer &GLBuffer::Data(GLsizei size, const void *data, GLenum usage) {
        Assert(handle, "null buffer handle");
        this->size = size;
        glNamedBufferData(handle, size, data, usage);
        return *this;
    }

    void *GLBuffer::Map(GLenum access) {
        Assert(handle, "null buffer handle");
        return glMapNamedBuffer(handle, access);
    }

    GLBuffer &GLBuffer::Unmap() {
        Assert(handle, "null buffer handle");
        glUnmapNamedBuffer(handle);
        return *this;
    }
} // namespace sp
