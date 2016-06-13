#pragma once

#include "Graphics.hh"

namespace sp
{
#define PF_DEFINITIONS \
	PF_DEF(PF_INVALID,           GL_NONE,               GL_NONE,            GL_NONE) \
	PF_DEF(PF_R8,                GL_R8,                 GL_RED,             GL_UNSIGNED_BYTE) \
	PF_DEF(PF_RG8,               GL_RG8,                GL_RG,              GL_UNSIGNED_BYTE) \
	PF_DEF(PF_RGB8,              GL_RGB8,               GL_RGB,             GL_UNSIGNED_BYTE) \
	PF_DEF(PF_SRGB8,             GL_SRGB8,              GL_RGB,             GL_UNSIGNED_BYTE) \
	PF_DEF(PF_RGB8F,             GL_RGB8,               GL_RGB,             GL_FLOAT) \
	PF_DEF(PF_RGBA8,             GL_RGBA8,              GL_RGBA,            GL_UNSIGNED_BYTE) \
	PF_DEF(PF_SRGB8_A8,          GL_SRGB8_ALPHA8,       GL_RGBA,            GL_UNSIGNED_BYTE) \
	PF_DEF(PF_RGBA16F,           GL_RGBA16F,            GL_RGBA,            GL_FLOAT) \
	PF_DEF(PF_RGBA32F,           GL_RGBA32F,            GL_RGBA,            GL_FLOAT) \
	PF_DEF(PF_DEPTH16,           GL_DEPTH_COMPONENT16,  GL_DEPTH_COMPONENT, GL_UNSIGNED_INT) \
	PF_DEF(PF_DEPTH24,           GL_DEPTH_COMPONENT24,  GL_DEPTH_COMPONENT, GL_UNSIGNED_INT) \
	PF_DEF(PF_DEPTH32F,          GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT) \
	PF_DEF(PF_DEPTH24_STENCIL8,  GL_DEPTH24_STENCIL8,   GL_DEPTH_STENCIL,   GL_UNSIGNED_INT_24_8) \
	PF_DEF(PF_DEPTH32F_STENCIL8, GL_DEPTH32F_STENCIL8,  GL_DEPTH_STENCIL,   GL_FLOAT_32_UNSIGNED_INT_24_8_REV)

#define PF_DEF(name, format, layout, type) name,

	enum PixelFormat
	{
		PF_DEFINITIONS
	};

	struct GLPixelFormat
	{
		GLPixelFormat() {}
		GLPixelFormat(GLenum i, GLenum f, GLenum t, bool preferSRGB = false)
			: internalFormat(i), format(f), type(t)
		{
			switch (i)
			{
				case GL_RGBA:
					internalFormat = preferSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
					break;
				case GL_RGB:
					internalFormat = preferSRGB ? GL_SRGB8 : GL_RGB8;
					break;
			}
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
