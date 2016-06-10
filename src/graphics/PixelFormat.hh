#pragma once

#include "Graphics.hh"

namespace sp
{
#define PF_DEFINITIONS \
	PF_DEF(PF_INVALID,            GL_NONE,               GL_NONE,            GL_NONE) \
	PF_DEF(PF_RGB8,               GL_RGB8,               GL_RGB,             GL_UNSIGNED_BYTE) \
	PF_DEF(PF_RGB8F,              GL_RGB8,               GL_RGB,             GL_FLOAT) \
	PF_DEF(PF_RGBA8,              GL_RGBA8,              GL_RGBA,            GL_UNSIGNED_BYTE) \
	PF_DEF(PF_RGBA16F,            GL_RGBA16F,            GL_RGBA,            GL_FLOAT) \
	PF_DEF(PF_RGBA32F,            GL_RGBA32F,            GL_RGBA,            GL_FLOAT) \
	PF_DEF(PF_DEPTH_COMPONENT16,  GL_DEPTH_COMPONENT16,  GL_DEPTH_COMPONENT, GL_UNSIGNED_INT) \
	PF_DEF(PF_DEPTH_COMPONENT24,  GL_DEPTH_COMPONENT24,  GL_DEPTH_COMPONENT, GL_UNSIGNED_INT) \
	PF_DEF(PF_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT)

#define PF_DEF(name, format, layout, type) name,

	enum PixelFormat
	{
		PF_DEFINITIONS
	};

	struct GLPixelFormat
	{
		GLPixelFormat() {}
		GLPixelFormat(GLenum i, GLenum f, GLenum t) : internalFormat(i), format(f), type(t)
		{
			if (i == GL_RGBA) internalFormat = GL_RGBA8;
			else if (i == GL_RGB) internalFormat = GL_RGB8;
		}

		GLenum internalFormat = GL_NONE;
		GLenum format = GL_NONE;
		GLenum type = GL_NONE;

		bool Valid() const
		{
			return internalFormat != GL_NONE && format != GL_NONE && type != GL_NONE;
		}

		static const GLPixelFormat PixelFormatMapping(PixelFormat in);
	};

#undef PF_DEF
}
