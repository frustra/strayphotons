#include "Buffer.hh"

namespace sp
{
	Buffer &Buffer::Create()
	{
		Assert(!handle);
		glCreateBuffers(1, &handle);
		return *this;
	}

	Buffer &Buffer::Delete()
	{
		if (handle)
			glDeleteBuffers(1, &handle);
		handle = 0;
		return *this;
	}

	void Buffer::Bind(GLenum target) const
	{
		Assert(handle);
		glBindBuffer(target, handle);
	}

	void Buffer::Bind(GLenum target, GLuint index, GLintptr offset, GLsizeiptr size) const
	{
		Assert(handle);
		Assert(this->size >= 0);
		if (size < 0) size = this->size - offset;
		glBindBufferRange(target, index, handle, offset, size);
	}

	Buffer &Buffer::Clear(GLPixelFormat format, const void *data)
	{
		Assert(handle);
		Assert(this->size >= 0);
		glClearNamedBufferData(handle, format.internalFormat, format.format, format.type, data);
		return *this;
	}

	Buffer &Buffer::Clear(PixelFormat format, const void *data)
	{
		return Clear(GLPixelFormat::PixelFormatMapping(format), data);
	}

	Buffer &Buffer::Data(GLsizei size, const void *data, GLenum usage)
	{
		Assert(handle);
		this->size = size;
		glNamedBufferData(handle, size, data, usage);
		return *this;
	}

	void *Buffer::Map(GLenum access)
	{
		Assert(handle);
		return glMapNamedBuffer(handle, access);
	}

	Buffer &Buffer::Unmap()
	{
		Assert(handle);
		glUnmapNamedBuffer(handle);
		return *this;
	}
}
