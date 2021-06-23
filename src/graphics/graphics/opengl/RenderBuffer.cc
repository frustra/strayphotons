#include "RenderBuffer.hh"

#include "core/Common.hh"

namespace sp {
    RenderBuffer &RenderBuffer::Create() {
        Assert(!handle, "renderbuffer cannot be recreated");
        glCreateRenderbuffers(1, &handle);
        return *this;
    }

    RenderBuffer &RenderBuffer::Delete() {
        if (handle) glDeleteRenderbuffers(1, &handle);
        handle = 0;
        return *this;
    }

    RenderBuffer &RenderBuffer::Size(GLsizei width, GLsizei height) {
        this->width = width;
        this->height = height;
        return *this;
    }

    RenderBuffer &RenderBuffer::Storage(GLPixelFormat format) {
        Assert(handle, "null renderbuffer handle");
        Assert(width && height, "renderbuffer size must be set before storage format");

        this->format = format;

        glNamedRenderbufferStorage(handle, this->format.internalFormat, width, height);

        return *this;
    }

    RenderBuffer &RenderBuffer::Storage(PixelFormat format) {
        return Storage(GLPixelFormat::PixelFormatMapping(format));
    }

    RenderBuffer &RenderBuffer::Storage(GLenum internalFormat, bool preferSRGB) {
        return Storage(GLPixelFormat(internalFormat, GL_NONE, GL_NONE, preferSRGB));
    }

    RenderBuffer &RenderBuffer::Attachment(GLenum attachment) {
        this->attachment = attachment;
        return *this;
    }
} // namespace sp
