#pragma once

#include "Graphics.hh"
#include "PixelFormat.hh"

namespace sp
{
	struct Texture
	{
		GLuint handle = 0;
		PixelFormat format = PF_INVALID;
		GLsizei width = 0, height = 0;

		void Delete()
		{
			glDeleteTextures(1, &handle);
		}

		void Bind(GLuint binding)
		{
			glBindTextures(binding, 1, &handle);
		}

		Texture Size(GLsizei width, GLsizei height)
		{
			this->width = width;
			this->height = height;
			return *this;
		}

		Texture Storage2D(PixelFormat format)
		{
			Assert(width && height);
			this->format = format;
			glTextureStorage2D(handle, 1, GLFormat().internalFormat, width, height);
			return *this;
		}

		Texture Image2D(const void *pixels, GLsizei subWidth = 0, GLsizei subHeight = 0, GLsizei xoffset = 0, GLsizei yoffset = 0, GLint level = 0)
		{
			Assert(pixels);
			Assert(width && height);
			Assert(format != PF_INVALID);

			if (subWidth == 0) subWidth = width;
			if (subHeight == 0) subHeight = height;

			glTextureSubImage2D(handle, level, xoffset, yoffset, subWidth, subHeight, GLFormat().format, GLFormat().type, pixels);
			return *this;
		}

		GLPixelFormat GLFormat()
		{
			return GLPixelFormat::PixelFormatMapping(format);
		}
	};

	// TODO(pushrax): stop being lazy
	static Texture NewTexture(
		GLenum target = GL_TEXTURE_2D,
		GLenum minFilter = GL_LINEAR,
		GLenum magFilter = GL_LINEAR,
		GLenum wrapS = GL_CLAMP_TO_EDGE,
		GLenum wrapT = GL_CLAMP_TO_EDGE)
	{
		Texture tex;
		glCreateTextures(target, 1, &tex.handle);
		glTextureParameteri(tex.handle, GL_TEXTURE_MIN_FILTER, minFilter);
		glTextureParameteri(tex.handle, GL_TEXTURE_MAG_FILTER, magFilter);
		glTextureParameteri(tex.handle, GL_TEXTURE_WRAP_S, wrapS);
		glTextureParameteri(tex.handle, GL_TEXTURE_WRAP_T, wrapT);
		return tex;
	}
}