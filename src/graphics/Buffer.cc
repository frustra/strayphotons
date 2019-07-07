#include "Buffer.hh"

namespace sp
{
	Buffer &Buffer::Create()
	{
		Assert(!handle, "buffer cannot be recreated");
		glCreateBuffers(1, &handle);
		return *this;
	}

	Buffer &Buffer::Delete()
	{
		if (handle)
			glDeleteBuffers(1, &handle);
		handle = 0;
		size = 0;
		return *this;
	}

	void Buffer::Bind(GLenum target) const
	{
		Assert(handle, "null buffer handle");
		glBindBuffer(target, handle);
	}

	void Buffer::Bind(GLenum target, GLuint index, GLintptr offset, GLsizeiptr size) const
	{
		Assert(handle, "null buffer handle");
		if (size < 0) size = this->size - offset;
		Assert(size >= 0, "binding offset is greater than size");
		glBindBufferRange(target, index, handle, offset, size);
	}

	Buffer &Buffer::ClearRegion(GLPixelFormat format, GLintptr offset, GLsizeiptr size, const void *data)
	{
		Assert(handle, "null buffer handle");
		if (size < 0) size = this->size - offset;
		Assert(size >= 0, "region offset is greater than size");
		glClearNamedBufferSubData(handle, format.internalFormat, offset, size, format.format, format.type, data);
		return *this;
	}

	Buffer &Buffer::ClearRegion(PixelFormat format, GLintptr offset, GLsizeiptr size, const void *data)
	{
		return ClearRegion(GLPixelFormat::PixelFormatMapping(format), offset, size, data);
	}

	Buffer &Buffer::Clear(GLPixelFormat format, const void *data)
	{
		Assert(handle, "null buffer handle");
		glClearNamedBufferData(handle, format.internalFormat, format.format, format.type, data);
		return *this;
	}

	Buffer &Buffer::Clear(PixelFormat format, const void *data)
	{
		return Clear(GLPixelFormat::PixelFormatMapping(format), data);
	}

	Buffer &Buffer::Data(GLsizei size, const void *data, GLenum usage)
	{
		Assert(handle, "null buffer handle");
		this->size = size;
		glNamedBufferData(handle, size, data, usage);
		return *this;
	}

	void *Buffer::Map(GLenum access)
	{
		Assert(handle, "null buffer handle");
		return glMapNamedBuffer(handle, access);
	}

	Buffer &Buffer::Unmap()
	{
		Assert(handle, "null buffer handle");
		glUnmapNamedBuffer(handle);
		return *this;
	}
}
