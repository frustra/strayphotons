#pragma once

#include "Graphics.hh"
#include "PixelFormat.hh"

namespace sp
{
	struct Buffer
	{
		GLuint handle = 0;
		GLsizei size = 0;

		Buffer &Create();
		Buffer &Delete();

		void Bind(GLenum target) const;
		void Bind(GLenum target, GLuint index, GLintptr offset = 0, GLsizeiptr size = -1) const;
		Buffer &Clear(GLPixelFormat format, const void *data = nullptr);
		Buffer &Clear(PixelFormat format, const void *data = nullptr);

		Buffer &Data(GLsizei size, const void *data = nullptr, GLenum usage = GL_STATIC_DRAW);

		bool operator==(const Buffer &other) const
		{
			return other.handle == handle;
		}

		bool operator!=(const Buffer &other) const
		{
			return !(*this == other);
		}

		bool operator!() const
		{
			return !handle;
		}

		operator bool() const
		{
			return !!handle;
		}
	};
}
