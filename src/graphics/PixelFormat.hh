#pragma once

#include "Graphics.hh"

namespace sp
{
#define PF_DEFINITIONS \
	PF_DEF(PF_INVALID,           GL_NONE,              GL_NONE,            GL_NONE) \
	PF_DEF(PF_RGB8,              GL_RGB8,              GL_RGB,             GL_UNSIGNED_BYTE) \
	PF_DEF(PF_RGBA8,             GL_RGBA8,             GL_RGBA,            GL_UNSIGNED_BYTE) \
	PF_DEF(PF_RGBA32F,           GL_RGBA32F,           GL_RGBA,            GL_FLOAT) \
	PF_DEF(PF_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT)

#define PF_DEF(name, format, layout, type) name = format,

	enum PixelFormat : GLenum
	{
		PF_DEFINITIONS
	};

	struct GLPixelFormat
	{
		GLPixelFormat(GLenum i, GLenum f, GLenum t) : internalFormat(i), format(f), type(t) { }

		GLenum internalFormat = GL_NONE;
		GLenum format = GL_NONE;
		GLenum type = GL_NONE;

		static const GLPixelFormat PixelFormatMapping(PixelFormat in);
	};

#undef PF_DEF
}